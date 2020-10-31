/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <android/fdsan.h>
#include <bionic/malloc.h>
#include <inttypes.h>
#include <malloc.h>
#include <private/bionic_malloc_dispatch.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <tuple>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/no_destructor.h"
#include "perfetto/ext/base/unix_socket.h"
#include "perfetto/ext/base/utils.h"
#include "src/profiling/common/proc_utils.h"
#include "src/profiling/memory/client.h"
#include "src/profiling/memory/scoped_spinlock.h"
#include "src/profiling/memory/unhooked_allocator.h"
#include "src/profiling/memory/wire_protocol.h"

using perfetto::profiling::ScopedSpinlock;
using perfetto::profiling::UnhookedAllocator;

// This is so we can make an so that we can swap out with the existing
// libc_malloc_hooks.so
#ifndef HEAPPROFD_PREFIX
#define HEAPPROFD_PREFIX heapprofd
#endif

#define HEAPPROFD_ADD_PREFIX(name) \
  PERFETTO_BUILDFLAG_CAT(HEAPPROFD_PREFIX, name)

#pragma GCC visibility push(default)
extern "C" {

bool HEAPPROFD_ADD_PREFIX(_initialize)(const MallocDispatch* malloc_dispatch,
                                       bool* zygote_child,
                                       const char* options);
void HEAPPROFD_ADD_PREFIX(_finalize)();
void HEAPPROFD_ADD_PREFIX(_dump_heap)(const char* file_name);
void HEAPPROFD_ADD_PREFIX(_get_malloc_leak_info)(uint8_t** info,
                                                 size_t* overall_size,
                                                 size_t* info_size,
                                                 size_t* total_memory,
                                                 size_t* backtrace_size);
bool HEAPPROFD_ADD_PREFIX(_write_malloc_leak_info)(FILE* fp);
ssize_t HEAPPROFD_ADD_PREFIX(_malloc_backtrace)(void* pointer,
                                                uintptr_t* frames,
                                                size_t frame_count);
void HEAPPROFD_ADD_PREFIX(_free_malloc_leak_info)(uint8_t* info);
size_t HEAPPROFD_ADD_PREFIX(_malloc_usable_size)(void* pointer);
void* HEAPPROFD_ADD_PREFIX(_malloc)(size_t size);
void HEAPPROFD_ADD_PREFIX(_free)(void* pointer);
void* HEAPPROFD_ADD_PREFIX(_aligned_alloc)(size_t alignment, size_t size);
void* HEAPPROFD_ADD_PREFIX(_memalign)(size_t alignment, size_t bytes);
void* HEAPPROFD_ADD_PREFIX(_realloc)(void* pointer, size_t bytes);
void* HEAPPROFD_ADD_PREFIX(_calloc)(size_t nmemb, size_t bytes);
struct mallinfo HEAPPROFD_ADD_PREFIX(_mallinfo)();
int HEAPPROFD_ADD_PREFIX(_mallopt)(int param, int value);
int HEAPPROFD_ADD_PREFIX(_malloc_info)(int options, FILE* fp);
int HEAPPROFD_ADD_PREFIX(_posix_memalign)(void** memptr,
                                          size_t alignment,
                                          size_t size);
int HEAPPROFD_ADD_PREFIX(_malloc_iterate)(uintptr_t base,
                                          size_t size,
                                          void (*callback)(uintptr_t base,
                                                           size_t size,
                                                           void* arg),
                                          void* arg);
void HEAPPROFD_ADD_PREFIX(_malloc_disable)();
void HEAPPROFD_ADD_PREFIX(_malloc_enable)();

#if defined(HAVE_DEPRECATED_MALLOC_FUNCS)
void* HEAPPROFD_ADD_PREFIX(_pvalloc)(size_t bytes);
void* HEAPPROFD_ADD_PREFIX(_valloc)(size_t size);
#endif
}
#pragma GCC visibility pop

namespace {

// The real malloc function pointers we get in initialize. Set once in the first
// initialize invocation, and never changed afterwards. Because bionic does a
// release write after initialization and an acquire read to retrieve the hooked
// malloc functions, we can use relaxed memory mode for both writing and
// reading.
std::atomic<const MallocDispatch*> g_dispatch{nullptr};

// Holds the active profiling client. Is empty at the start, or after we've
// started shutting down a profiling session. Hook invocations take shared_ptr
// copies (ensuring that the client stays alive until no longer needed), and do
// nothing if this master pointer is empty.
//
// This shared_ptr itself is protected by g_client_lock. Note that shared_ptr
// handles are not thread-safe by themselves:
// https://en.cppreference.com/w/cpp/memory/shared_ptr/atomic
//
// To avoid on-destruction re-entrancy issues, this shared_ptr needs to be
// constructed with an allocator that uses the unhooked malloc & free functions.
// See UnhookedAllocator.
//
// NoDestructor<> wrapper is used to avoid destructing the shared_ptr at program
// exit. The rationale is:
// * Avoiding the atexit destructor racing against other threads that are
//   possibly running within the hooks.
// * Making sure that atexit handlers running after this global's destructor
//   can still safely enter the hooks.
perfetto::base::NoDestructor<std::shared_ptr<perfetto::profiling::Client>>
    g_client;

// Protects g_client, and serves as an external lock for sampling decisions (see
// perfetto::profiling::Sampler).
//
// We rely on this atomic's destuction being a nop, as it is possible for the
// hooks to attempt to acquire the spinlock after its destructor should have run
// (technically a use-after-destruct scenario).
std::atomic<bool> g_client_lock{false};

constexpr char kHeapprofdBinPath[] = "/system/bin/heapprofd";

const MallocDispatch* GetDispatch() {
  return g_dispatch.load(std::memory_order_relaxed);
}

int CloneWithoutSigchld() {
  auto ret = clone(nullptr, nullptr, 0, nullptr);
  if (ret == 0)
    android_fdsan_set_error_level(ANDROID_FDSAN_ERROR_LEVEL_DISABLED);
  return ret;
}

int ForklikeClone() {
  auto ret = clone(nullptr, nullptr, SIGCHLD, nullptr);
  if (ret == 0)
    android_fdsan_set_error_level(ANDROID_FDSAN_ERROR_LEVEL_DISABLED);
  return ret;
}

// Like daemon(), but using clone to avoid invoking pthread_atfork(3) handlers.
int Daemonize() {
  switch (ForklikeClone()) {
    case -1:
      PERFETTO_PLOG("Daemonize.clone");
      return -1;
      break;
    case 0:
      break;
    default:
      _exit(0);
      break;
  }
  if (setsid() == -1) {
    PERFETTO_PLOG("Daemonize.setsid");
    return -1;
  }
  // best effort chdir & fd close
  chdir("/");
  int fd = open("/dev/null", O_RDWR, 0);
  if (fd != -1) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO)
      close(fd);
  }
  return 0;
}

// Called only if |g_client_lock| acquisition fails, which shouldn't happen
// unless we're in a completely unexpected state (which we won't know how to
// recover from). Tries to abort (SIGABRT) the whole process to serve as an
// explicit indication of a bug.
//
// Doesn't use PERFETTO_FATAL as that is a single attempt to self-signal (in
// practice - SIGTRAP), while abort() tries to make sure the process has
// exited one way or another.
__attribute__((noreturn, noinline)) void AbortOnSpinlockTimeout() {
  PERFETTO_ELOG(
      "Timed out on the spinlock - something is horribly wrong. "
      "Aborting whole process.");
  abort();
}

std::string ReadSystemProperty(const char* key) {
  std::string prop_value;
  const prop_info* prop = __system_property_find(key);
  if (!prop) {
    return prop_value;  // empty
  }
  __system_property_read_callback(
      prop,
      [](void* cookie, const char* name, const char* value, uint32_t) {
        std::string* prop_value = reinterpret_cast<std::string*>(cookie);
        *prop_value = value;
      },
      &prop_value);
  return prop_value;
}

bool ForceForkPrivateDaemon() {
  // Note: if renaming the property, also update system_property.cc
  std::string mode = ReadSystemProperty("heapprofd.userdebug.mode");
  return mode == "fork";
}

std::shared_ptr<perfetto::profiling::Client> CreateClientForCentralDaemon(
    UnhookedAllocator<perfetto::profiling::Client> unhooked_allocator) {
  PERFETTO_LOG("Constructing client for central daemon.");
  using perfetto::profiling::Client;

  perfetto::base::Optional<perfetto::base::UnixSocketRaw> sock =
      Client::ConnectToHeapprofd(perfetto::profiling::kHeapprofdSocketFile);
  if (!sock) {
    PERFETTO_ELOG("Failed to connect to %s. This is benign on user builds.",
                  perfetto::profiling::kHeapprofdSocketFile);
    return nullptr;
  }
  return Client::CreateAndHandshake(std::move(sock.value()),
                                    unhooked_allocator);
}

std::shared_ptr<perfetto::profiling::Client> CreateClientAndPrivateDaemon(
    UnhookedAllocator<perfetto::profiling::Client> unhooked_allocator) {
  PERFETTO_LOG("Setting up fork mode profiling.");
  perfetto::base::UnixSocketRaw parent_sock;
  perfetto::base::UnixSocketRaw child_sock;
  std::tie(parent_sock, child_sock) = perfetto::base::UnixSocketRaw::CreatePair(
      perfetto::base::SockFamily::kUnix, perfetto::base::SockType::kStream);

  if (!parent_sock || !child_sock) {
    PERFETTO_PLOG("Failed to create socketpair.");
    return nullptr;
  }

  child_sock.RetainOnExec();

  // Record own pid and cmdline, to pass down to the forked heapprofd.
  pid_t target_pid = getpid();
  std::string target_cmdline;
  if (!perfetto::profiling::GetCmdlineForPID(target_pid, &target_cmdline)) {
    target_cmdline = "failed-to-read-cmdline";
    PERFETTO_ELOG(
        "Failed to read own cmdline, proceeding as this might be a by-pid "
        "profiling request (which will still work).");
  }

  // Prepare arguments for heapprofd.
  std::string pid_arg =
      std::string("--exclusive-for-pid=") + std::to_string(target_pid);
  std::string cmd_arg =
      std::string("--exclusive-for-cmdline=") + target_cmdline;
  std::string fd_arg =
      std::string("--inherit-socket-fd=") + std::to_string(child_sock.fd());
  const char* argv[] = {kHeapprofdBinPath, pid_arg.c_str(), cmd_arg.c_str(),
                        fd_arg.c_str(), nullptr};

  // Use fork-like clone to avoid invoking the host's pthread_atfork(3)
  // handlers. Also avoid sending the current process a SIGCHILD to further
  // reduce our interference.
  pid_t clone_pid = CloneWithoutSigchld();
  if (clone_pid == -1) {
    PERFETTO_PLOG("Failed to clone.");
    return nullptr;
  }
  if (clone_pid == 0) {  // child
    // Daemonize clones again, terminating the calling thread (i.e. the direct
    // child of the original process). So the rest of this codepath will be
    // executed in a new reparented process.
    if (Daemonize() == -1) {
      PERFETTO_PLOG("Daemonization failed.");
      _exit(1);
    }
    execv(kHeapprofdBinPath, const_cast<char**>(argv));
    PERFETTO_PLOG("Failed to execute private heapprofd.");
    _exit(1);
  }  // else - parent continuing the client setup

  child_sock.ReleaseFd().reset();  // close child socket's fd
  if (!parent_sock.SetTxTimeout(perfetto::profiling::kClientSockTimeoutMs)) {
    PERFETTO_PLOG("Failed to set socket transmit timeout.");
    return nullptr;
  }

  if (!parent_sock.SetRxTimeout(perfetto::profiling::kClientSockTimeoutMs)) {
    PERFETTO_PLOG("Failed to set socket receive timeout.");
    return nullptr;
  }

  // Wait on the immediate child to exit (allow for ECHILD in the unlikely case
  // we're in a process that has made its children unwaitable).
  int unused = 0;
  if (PERFETTO_EINTR(waitpid(clone_pid, &unused, __WCLONE)) == -1 &&
      errno != ECHILD) {
    PERFETTO_PLOG("Failed to waitpid on immediate child.");
    return nullptr;
  }

  return perfetto::profiling::Client::CreateAndHandshake(std::move(parent_sock),
                                                         unhooked_allocator);
}

// Note: android_mallopt(M_RESET_HOOKS) is mutually exclusive with
// heapprofd_initialize. Concurrent calls get discarded, which might be our
// unpatching attempt if there is a concurrent re-initialization running due to
// a new signal.
//
// Note: g_client can be reset by heapprofd_initialize without calling this
// function.
void ShutdownLazy() {
  ScopedSpinlock s(&g_client_lock, ScopedSpinlock::Mode::Try);
  if (PERFETTO_UNLIKELY(!s.locked()))
    AbortOnSpinlockTimeout();

  if (!g_client.ref())  // other invocation already initiated shutdown
    return;

  // Clear primary shared pointer, such that later hook invocations become nops.
  g_client.ref().reset();

  if (!android_mallopt(M_RESET_HOOKS, nullptr, 0))
    PERFETTO_PLOG("Unpatching heapprofd hooks failed.");
}

}  // namespace

// Setup for the rest of profiling. The first time profiling is triggered in a
// process, this is called after this client library is dlopened, but before the
// rest of the hooks are patched in. However, as we support multiple profiling
// sessions within a process' lifetime, this function can also be legitimately
// called any number of times afterwards (note: bionic guarantees that at most
// one initialize call is active at a time).
//
// Note: if profiling is triggered at runtime, this runs on a dedicated pthread
// (which is safe to block). If profiling is triggered at startup, then this
// code runs synchronously.
bool HEAPPROFD_ADD_PREFIX(_initialize)(const MallocDispatch* malloc_dispatch,
                                       bool*,
                                       const char*) {
  using ::perfetto::profiling::Client;

  // Table of pointers to backing implementation.
  g_dispatch.store(malloc_dispatch, std::memory_order_relaxed);

  // TODO(fmayer): Check other destructions of client and make a decision
  // whether we want to ban heap objects in the client or not.
  std::shared_ptr<Client> old_client;
  {
    ScopedSpinlock s(&g_client_lock, ScopedSpinlock::Mode::Try);
    if (PERFETTO_UNLIKELY(!s.locked()))
      AbortOnSpinlockTimeout();

    if (g_client.ref()) {
      PERFETTO_LOG("%s: Rejecting concurrent profiling initialization.",
                   getprogname());
      return true;  // success as we're in a valid state
    }
    old_client = g_client.ref();
    g_client.ref().reset();
  }

  old_client.reset();

  // The dispatch table never changes, so let the custom allocator retain the
  // function pointers directly.
  UnhookedAllocator<Client> unhooked_allocator(malloc_dispatch->malloc,
                                               malloc_dispatch->free);

  // These factory functions use heap objects, so we need to run them without
  // the spinlock held.
  std::shared_ptr<Client> client;
  if (!ForceForkPrivateDaemon())
    client = CreateClientForCentralDaemon(unhooked_allocator);
  if (!client)
    client = CreateClientAndPrivateDaemon(unhooked_allocator);

  if (!client) {
    PERFETTO_LOG("%s: heapprofd_client not initialized, not installing hooks.",
                 getprogname());
    return false;
  }
  PERFETTO_LOG("%s: heapprofd_client initialized.", getprogname());
  {
    ScopedSpinlock s(&g_client_lock, ScopedSpinlock::Mode::Try);
    if (PERFETTO_UNLIKELY(!s.locked()))
      AbortOnSpinlockTimeout();

    // This cannot have been set in the meantime. There are never two concurrent
    // calls to this function, as Bionic uses atomics to guard against that.
    PERFETTO_DCHECK(g_client.ref() == nullptr);
    g_client.ref() = std::move(client);
  }
  return true;
}

void HEAPPROFD_ADD_PREFIX(_finalize)() {
  // At the time of writing, invoked only as an atexit handler. We don't have
  // any specific action to take, and cleanup can be left to the OS.
}

// Decides whether an allocation with the given address and size needs to be
// sampled, and if so, records it. Performs the necessary synchronization (holds
// |g_client_lock| spinlock) while accessing the shared sampler, and obtaining a
// profiling client handle (shared_ptr).
//
// If the allocation is to be sampled, the recording is done without holding
// |g_client_lock|. The client handle is guaranteed to not be invalidated while
// the allocation is being recorded.
//
// If the attempt to record the allocation fails, initiates lazy shutdown of the
// client & hooks.
static void MaybeSampleAllocation(size_t size, void* addr) {
  size_t sampled_alloc_sz = 0;
  std::shared_ptr<perfetto::profiling::Client> client;
  {
    ScopedSpinlock s(&g_client_lock, ScopedSpinlock::Mode::Try);
    if (PERFETTO_UNLIKELY(!s.locked()))
      AbortOnSpinlockTimeout();

    if (!g_client.ref())  // no active client (most likely shutting down)
      return;

    sampled_alloc_sz = g_client.ref()->GetSampleSizeLocked(size);
    if (sampled_alloc_sz == 0)  // not sampling
      return;

    client = g_client.ref();  // owning copy
  }                           // unlock

  if (!client->RecordMalloc(sampled_alloc_sz, size,
                            reinterpret_cast<uint64_t>(addr))) {
    ShutdownLazy();
  }
}

void* HEAPPROFD_ADD_PREFIX(_malloc)(size_t size) {
  const MallocDispatch* dispatch = GetDispatch();
  void* addr = dispatch->malloc(size);
  MaybeSampleAllocation(size, addr);
  return addr;
}

void* HEAPPROFD_ADD_PREFIX(_calloc)(size_t nmemb, size_t size) {
  const MallocDispatch* dispatch = GetDispatch();
  void* addr = dispatch->calloc(nmemb, size);
  MaybeSampleAllocation(nmemb * size, addr);
  return addr;
}

void* HEAPPROFD_ADD_PREFIX(_aligned_alloc)(size_t alignment, size_t size) {
  const MallocDispatch* dispatch = GetDispatch();
  void* addr = dispatch->aligned_alloc(alignment, size);
  MaybeSampleAllocation(size, addr);
  return addr;
}

void* HEAPPROFD_ADD_PREFIX(_memalign)(size_t alignment, size_t size) {
  const MallocDispatch* dispatch = GetDispatch();
  void* addr = dispatch->memalign(alignment, size);
  MaybeSampleAllocation(size, addr);
  return addr;
}

int HEAPPROFD_ADD_PREFIX(_posix_memalign)(void** memptr,
                                          size_t alignment,
                                          size_t size) {
  const MallocDispatch* dispatch = GetDispatch();
  int res = dispatch->posix_memalign(memptr, alignment, size);
  if (res != 0)
    return res;

  MaybeSampleAllocation(size, *memptr);
  return 0;
}

// Note: we record the free before calling the backing implementation to make
// sure that the address is not reused before we've processed the deallocation
// (which includes assigning a sequence id to it).
void HEAPPROFD_ADD_PREFIX(_free)(void* pointer) {
  // free on a nullptr is valid but has no effect. Short circuit here, for
  // various advantages:
  // * More efficient
  // * Notably printf calls free(nullptr) even when it is used in a way
  //   malloc-free way, as it unconditionally frees the pointer even if
  //   it was never written to.
  //   Short circuiting here makes it less likely to accidentally build
  //   infinite recursion.
  if (pointer == nullptr)
    return;

  const MallocDispatch* dispatch = GetDispatch();
  std::shared_ptr<perfetto::profiling::Client> client;
  {
    ScopedSpinlock s(&g_client_lock, ScopedSpinlock::Mode::Try);
    if (PERFETTO_UNLIKELY(!s.locked()))
      AbortOnSpinlockTimeout();

    client = g_client.ref();  // owning copy (or empty)
  }

  if (client) {
    if (!client->RecordFree(reinterpret_cast<uint64_t>(pointer)))
      ShutdownLazy();
  }
  return dispatch->free(pointer);
}

// Approach to recording realloc: under the initial lock, get a safe copy of the
// client, and make the sampling decision in advance. Then record the
// deallocation, call the real realloc, and finally record the sample if one is
// necessary.
//
// As with the free, we record the deallocation before calling the backing
// implementation to make sure the address is still exclusive while we're
// processing it.
void* HEAPPROFD_ADD_PREFIX(_realloc)(void* pointer, size_t size) {
  const MallocDispatch* dispatch = GetDispatch();

  size_t sampled_alloc_sz = 0;
  std::shared_ptr<perfetto::profiling::Client> client;
  {
    ScopedSpinlock s(&g_client_lock, ScopedSpinlock::Mode::Try);
    if (PERFETTO_UNLIKELY(!s.locked()))
      AbortOnSpinlockTimeout();

    // If there is no active client, we still want to reach the backing realloc,
    // so keep going.
    if (g_client.ref()) {
      client = g_client.ref();  // owning copy
      sampled_alloc_sz = g_client.ref()->GetSampleSizeLocked(size);
    }
  }  // unlock

  if (client && pointer) {
    if (!client->RecordFree(reinterpret_cast<uint64_t>(pointer)))
      ShutdownLazy();
  }
  void* addr = dispatch->realloc(pointer, size);

  if (size == 0 || sampled_alloc_sz == 0)
    return addr;

  // We do not reach this point without a valid client, because in that case
  // sampled_alloc_sz == 0.
  PERFETTO_DCHECK(client);

  if (!client->RecordMalloc(sampled_alloc_sz, size,
                            reinterpret_cast<uint64_t>(addr))) {
    ShutdownLazy();
  }
  return addr;
}

void HEAPPROFD_ADD_PREFIX(_dump_heap)(const char*) {}

void HEAPPROFD_ADD_PREFIX(
    _get_malloc_leak_info)(uint8_t**, size_t*, size_t*, size_t*, size_t*) {}

bool HEAPPROFD_ADD_PREFIX(_write_malloc_leak_info)(FILE*) {
  return false;
}

ssize_t HEAPPROFD_ADD_PREFIX(_malloc_backtrace)(void*, uintptr_t*, size_t) {
  return -1;
}

void HEAPPROFD_ADD_PREFIX(_free_malloc_leak_info)(uint8_t*) {}

size_t HEAPPROFD_ADD_PREFIX(_malloc_usable_size)(void* pointer) {
  const MallocDispatch* dispatch = GetDispatch();
  return dispatch->malloc_usable_size(pointer);
}

struct mallinfo HEAPPROFD_ADD_PREFIX(_mallinfo)() {
  const MallocDispatch* dispatch = GetDispatch();
  return dispatch->mallinfo();
}

int HEAPPROFD_ADD_PREFIX(_mallopt)(int param, int value) {
  const MallocDispatch* dispatch = GetDispatch();
  return dispatch->mallopt(param, value);
}

int HEAPPROFD_ADD_PREFIX(_malloc_info)(int options, FILE* fp) {
  const MallocDispatch* dispatch = GetDispatch();
  return dispatch->malloc_info(options, fp);
}

int HEAPPROFD_ADD_PREFIX(_malloc_iterate)(uintptr_t base,
                                          size_t size,
                                          void (*callback)(uintptr_t base,
                                                   size_t size,
                                                   void* arg),
                                          void* arg) {
  const MallocDispatch* dispatch = GetDispatch();
  return dispatch->malloc_iterate(base, size, callback, arg);
}

void HEAPPROFD_ADD_PREFIX(_malloc_disable)() {
  const MallocDispatch* dispatch = GetDispatch();
  return dispatch->malloc_disable();
}

void HEAPPROFD_ADD_PREFIX(_malloc_enable)() {
  const MallocDispatch* dispatch = GetDispatch();
  return dispatch->malloc_enable();
}

#if defined(HAVE_DEPRECATED_MALLOC_FUNCS)
void* HEAPPROFD_ADD_PREFIX(_pvalloc)(size_t size) {
  const MallocDispatch* dispatch = GetDispatch();
  return dispatch->pvalloc(size);
}

void* HEAPPROFD_ADD_PREFIX(_valloc)(size_t size) {
  const MallocDispatch* dispatch = GetDispatch();
  return dispatch->valloc(size);
}

#endif
