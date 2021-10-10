// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_INITIALIZATION_H_
#define INCLUDE_V8_INITIALIZATION_H_

#include <stddef.h>
#include <stdint.h>

#include "v8-internal.h"  // NOLINT(build/include_directory)
#include "v8-isolate.h"   // NOLINT(build/include_directory)
#include "v8-platform.h"  // NOLINT(build/include_directory)
#include "v8config.h"     // NOLINT(build/include_directory)

// We reserve the V8_* prefix for macros defined in V8 public API and
// assume there are no name conflicts with the embedder's code.

/**
 * The v8 JavaScript engine.
 */
namespace v8 {

class PageAllocator;
class Platform;
template <class K, class V, class T>
class PersistentValueMapBase;

/**
 * EntropySource is used as a callback function when v8 needs a source
 * of entropy.
 */
using EntropySource = bool (*)(unsigned char* buffer, size_t length);

/**
 * ReturnAddressLocationResolver is used as a callback function when v8 is
 * resolving the location of a return address on the stack. Profilers that
 * change the return address on the stack can use this to resolve the stack
 * location to wherever the profiler stashed the original return address.
 *
 * \param return_addr_location A location on stack where a machine
 *    return address resides.
 * \returns Either return_addr_location, or else a pointer to the profiler's
 *    copy of the original return address.
 *
 * \note The resolver function must not cause garbage collection.
 */
using ReturnAddressLocationResolver =
    uintptr_t (*)(uintptr_t return_addr_location);

using DcheckErrorCallback = void (*)(const char* file, int line,
                                     const char* message);

/**
 * Container class for static utility functions.
 */
class V8_EXPORT V8 {
 public:
  /**
   * Hand startup data to V8, in case the embedder has chosen to build
   * V8 with external startup data.
   *
   * Note:
   * - By default the startup data is linked into the V8 library, in which
   *   case this function is not meaningful.
   * - If this needs to be called, it needs to be called before V8
   *   tries to make use of its built-ins.
   * - To avoid unnecessary copies of data, V8 will point directly into the
   *   given data blob, so pretty please keep it around until V8 exit.
   * - Compression of the startup blob might be useful, but needs to
   *   handled entirely on the embedders' side.
   * - The call will abort if the data is invalid.
   */
  static void SetSnapshotDataBlob(StartupData* startup_blob);

  /** Set the callback to invoke in case of Dcheck failures. */
  static void SetDcheckErrorHandler(DcheckErrorCallback that);

  /**
   * Sets V8 flags from a string.
   */
  static void SetFlagsFromString(const char* str);
  static void SetFlagsFromString(const char* str, size_t length);

  /**
   * Sets V8 flags from the command line.
   */
  static void SetFlagsFromCommandLine(int* argc, char** argv,
                                      bool remove_flags);

  /** Get the version string. */
  static const char* GetVersion();

  /**
   * Initializes V8. This function needs to be called before the first Isolate
   * is created. It always returns true.
   */
  V8_INLINE static bool Initialize() {
    const int kBuildConfiguration =
        (internal::PointerCompressionIsEnabled() ? kPointerCompression : 0) |
        (internal::SmiValuesAre31Bits() ? k31BitSmis : 0) |
        (internal::HeapSandboxIsEnabled() ? kHeapSandbox : 0) |
        (internal::VirtualMemoryCageIsEnabled() ? kVirtualMemoryCage : 0);
    return Initialize(kBuildConfiguration);
  }

  /**
   * Allows the host application to provide a callback which can be used
   * as a source of entropy for random number generators.
   */
  static void SetEntropySource(EntropySource source);

  /**
   * Allows the host application to provide a callback that allows v8 to
   * cooperate with a profiler that rewrites return addresses on stack.
   */
  static void SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver return_address_resolver);

  /**
   * Releases any resources used by v8 and stops any utility threads
   * that may be running.  Note that disposing v8 is permanent, it
   * cannot be reinitialized.
   *
   * It should generally not be necessary to dispose v8 before exiting
   * a process, this should happen automatically.  It is only necessary
   * to use if the process needs the resources taken up by v8.
   */
  static bool Dispose();

  /**
   * Initialize the ICU library bundled with V8. The embedder should only
   * invoke this method when using the bundled ICU. Returns true on success.
   *
   * If V8 was compiled with the ICU data in an external file, the location
   * of the data file has to be provided.
   */
  static bool InitializeICU(const char* icu_data_file = nullptr);

  /**
   * Initialize the ICU library bundled with V8. The embedder should only
   * invoke this method when using the bundled ICU. If V8 was compiled with
   * the ICU data in an external file and when the default location of that
   * file should be used, a path to the executable must be provided.
   * Returns true on success.
   *
   * The default is a file called icudtl.dat side-by-side with the executable.
   *
   * Optionally, the location of the data file can be provided to override the
   * default.
   */
  static bool InitializeICUDefaultLocation(const char* exec_path,
                                           const char* icu_data_file = nullptr);

  /**
   * Initialize the external startup data. The embedder only needs to
   * invoke this method when external startup data was enabled in a build.
   *
   * If V8 was compiled with the startup data in an external file, then
   * V8 needs to be given those external files during startup. There are
   * three ways to do this:
   * - InitializeExternalStartupData(const char*)
   *   This will look in the given directory for the file "snapshot_blob.bin".
   * - InitializeExternalStartupDataFromFile(const char*)
   *   As above, but will directly use the given file name.
   * - Call SetSnapshotDataBlob.
   *   This will read the blobs from the given data structure and will
   *   not perform any file IO.
   */
  static void InitializeExternalStartupData(const char* directory_path);
  static void InitializeExternalStartupDataFromFile(const char* snapshot_blob);

  /**
   * Sets the v8::Platform to use. This should be invoked before V8 is
   * initialized.
   */
  static void InitializePlatform(Platform* platform);

  /**
   * Clears all references to the v8::Platform. This should be invoked after
   * V8 was disposed.
   */
  static void ShutdownPlatform();

#ifdef V8_VIRTUAL_MEMORY_CAGE
  //
  // Virtual Memory Cage related API.
  //
  // This API is not yet stable and subject to changes in the future.
  //

  /**
   * Initializes the virtual memory cage for V8.
   *
   * This must be invoked after the platform was initialized but before V8 is
   * initialized. The virtual memory cage is torn down during platform shutdown.
   * Returns true on success, false otherwise.
   */
  static bool InitializeVirtualMemoryCage();

  /**
   * Provides access to the data page allocator for the virtual memory cage.
   *
   * This allocator allocates pages inside the data cage part of the virtual
   * memory cage in which data buffers such as ArrayBuffer backing stores must
   * be allocated. Objects in this region should generally consists purely of
   * data and not contain any pointers. It should be assumed that an attacker
   * can corrupt data inside the cage, and so in particular the contents of
   * pages returned by this allocator, arbitrarily and concurrently.
   *
   * The virtual memory cage must have been initialized before.
   */
  static PageAllocator* GetVirtualMemoryCageDataPageAllocator();
#endif

  /**
   * Activate trap-based bounds checking for WebAssembly.
   *
   * \param use_v8_signal_handler Whether V8 should install its own signal
   * handler or rely on the embedder's.
   */
  static bool EnableWebAssemblyTrapHandler(bool use_v8_signal_handler);

#if defined(V8_OS_WIN)
  /**
   * On Win64, by default V8 does not emit unwinding data for jitted code,
   * which means the OS cannot walk the stack frames and the system Structured
   * Exception Handling (SEH) cannot unwind through V8-generated code:
   * https://code.google.com/p/v8/issues/detail?id=3598.
   *
   * This function allows embedders to register a custom exception handler for
   * exceptions in V8-generated code.
   */
  static void SetUnhandledExceptionCallback(
      UnhandledExceptionCallback unhandled_exception_callback);
#endif

  /**
   * Get statistics about the shared memory usage.
   */
  static void GetSharedMemoryStatistics(SharedMemoryStatistics* statistics);

 private:
  V8();

  enum BuildConfigurationFeatures {
    kPointerCompression = 1 << 0,
    k31BitSmis = 1 << 1,
    kHeapSandbox = 1 << 2,
    kVirtualMemoryCage = 1 << 3,
  };

  /**
   * Checks that the embedder build configuration is compatible with
   * the V8 binary and if so initializes V8.
   */
  static bool Initialize(int build_config);

  friend class Context;
  template <class K, class V, class T>
  friend class PersistentValueMapBase;
};

}  // namespace v8

#endif  // INCLUDE_V8_INITIALIZATION_H_
