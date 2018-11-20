// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for Cygwin goes here. For the POSIX-compatible
// parts, the implementation is in platform-posix.cc.

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <strings.h>    // index
#include <sys/mman.h>   // mmap & munmap
#include <sys/time.h>
#include <unistd.h>     // sysconf

#include <cmath>

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/win32-headers.h"

namespace v8 {
namespace base {


const char* OS::LocalTimezone(double time, TimezoneCache* cache) {
  if (std::isnan(time)) return "";
  time_t tv = static_cast<time_t>(std::floor(time/msPerSecond));
  struct tm* t = localtime(&tv);
  if (NULL == t) return "";
  return tzname[0];  // The location of the timezone string on Cygwin.
}


double OS::LocalTimeOffset(TimezoneCache* cache) {
  // On Cygwin, struct tm does not contain a tm_gmtoff field.
  time_t utc = time(NULL);
  DCHECK(utc != -1);
  struct tm* loc = localtime(&utc);
  DCHECK(loc != NULL);
  // time - localtime includes any daylight savings offset, so subtract it.
  return static_cast<double>((mktime(loc) - utc) * msPerSecond -
                             (loc->tm_isdst > 0 ? 3600 * msPerSecond : 0));
}


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  const size_t msize = RoundUp(requested, sysconf(_SC_PAGESIZE));
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void* mbase = mmap(NULL, msize, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mbase == MAP_FAILED) return NULL;
  *allocated = msize;
  return mbase;
}


class PosixMemoryMappedFile : public OS::MemoryMappedFile {
 public:
  PosixMemoryMappedFile(FILE* file, void* memory, int size)
    : file_(file), memory_(memory), size_(size) { }
  virtual ~PosixMemoryMappedFile();
  virtual void* memory() { return memory_; }
  virtual int size() { return size_; }
 private:
  FILE* file_;
  void* memory_;
  int size_;
};


OS::MemoryMappedFile* OS::MemoryMappedFile::open(const char* name) {
  FILE* file = fopen(name, "r+");
  if (file == NULL) return NULL;

  fseek(file, 0, SEEK_END);
  int size = ftell(file);

  void* memory =
      mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
  return new PosixMemoryMappedFile(file, memory, size);
}


OS::MemoryMappedFile* OS::MemoryMappedFile::create(const char* name, int size,
    void* initial) {
  FILE* file = fopen(name, "w+");
  if (file == NULL) return NULL;
  int result = fwrite(initial, size, 1, file);
  if (result < 1) {
    fclose(file);
    return NULL;
  }
  void* memory =
      mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
  return new PosixMemoryMappedFile(file, memory, size);
}


PosixMemoryMappedFile::~PosixMemoryMappedFile() {
  if (memory_) munmap(memory_, size_);
  fclose(file_);
}


std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  std::vector<SharedLibraryAddresses> result;
  // This function assumes that the layout of the file is as follows:
  // hex_start_addr-hex_end_addr rwxp <unused data> [binary_file_name]
  // If we encounter an unexpected situation we abort scanning further entries.
  FILE* fp = fopen("/proc/self/maps", "r");
  if (fp == NULL) return result;

  // Allocate enough room to be able to store a full file name.
  const int kLibNameLen = FILENAME_MAX + 1;
  char* lib_name = reinterpret_cast<char*>(malloc(kLibNameLen));

  // This loop will terminate once the scanning hits an EOF.
  while (true) {
    uintptr_t start, end;
    char attr_r, attr_w, attr_x, attr_p;
    // Parse the addresses and permission bits at the beginning of the line.
    if (fscanf(fp, "%" V8PRIxPTR "-%" V8PRIxPTR, &start, &end) != 2) break;
    if (fscanf(fp, " %c%c%c%c", &attr_r, &attr_w, &attr_x, &attr_p) != 4) break;

    int c;
    if (attr_r == 'r' && attr_w != 'w' && attr_x == 'x') {
      // Found a read-only executable entry. Skip characters until we reach
      // the beginning of the filename or the end of the line.
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\n') && (c != '/'));
      if (c == EOF) break;  // EOF: Was unexpected, just exit.

      // Process the filename if found.
      if (c == '/') {
        ungetc(c, fp);  // Push the '/' back into the stream to be read below.

        // Read to the end of the line. Exit if the read fails.
        if (fgets(lib_name, kLibNameLen, fp) == NULL) break;

        // Drop the newline character read by fgets. We do not need to check
        // for a zero-length string because we know that we at least read the
        // '/' character.
        lib_name[strlen(lib_name) - 1] = '\0';
      } else {
        // No library name found, just record the raw address range.
        snprintf(lib_name, kLibNameLen,
                 "%08" V8PRIxPTR "-%08" V8PRIxPTR, start, end);
      }
      result.push_back(SharedLibraryAddress(lib_name, start, end));
    } else {
      // Entry not describing executable data. Skip to end of line to set up
      // reading the next entry.
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\n'));
      if (c == EOF) break;
    }
  }
  free(lib_name);
  fclose(fp);
  return result;
}


void OS::SignalCodeMovingGC() {
  // Nothing to do on Cygwin.
}


// The VirtualMemory implementation is taken from platform-win32.cc.
// The mmap-based virtual memory implementation as it is used on most posix
// platforms does not work well because Cygwin does not support MAP_FIXED.
// This causes VirtualMemory::Commit to not always commit the memory region
// specified.

static void* RandomizedVirtualAlloc(size_t size, int action, int protection) {
  LPVOID base = NULL;

  if (protection == PAGE_EXECUTE_READWRITE || protection == PAGE_NOACCESS) {
    // For exectutable pages try and randomize the allocation address
    for (size_t attempts = 0; base == NULL && attempts < 3; ++attempts) {
      base = VirtualAlloc(OS::GetRandomMmapAddr(), size, action, protection);
    }
  }

  // After three attempts give up and let the OS find an address to use.
  if (base == NULL) base = VirtualAlloc(NULL, size, action, protection);

  return base;
}


VirtualMemory::VirtualMemory() : address_(NULL), size_(0) { }


VirtualMemory::VirtualMemory(size_t size)
    : address_(ReserveRegion(size)), size_(size) { }


VirtualMemory::VirtualMemory(size_t size, size_t alignment)
    : address_(NULL), size_(0) {
  DCHECK((alignment % OS::AllocateAlignment()) == 0);
  size_t request_size = RoundUp(size + alignment,
                                static_cast<intptr_t>(OS::AllocateAlignment()));
  void* address = ReserveRegion(request_size);
  if (address == NULL) return;
  uint8_t* base = RoundUp(static_cast<uint8_t*>(address), alignment);
  // Try reducing the size by freeing and then reallocating a specific area.
  bool result = ReleaseRegion(address, request_size);
  USE(result);
  DCHECK(result);
  address = VirtualAlloc(base, size, MEM_RESERVE, PAGE_NOACCESS);
  if (address != NULL) {
    request_size = size;
    DCHECK(base == static_cast<uint8_t*>(address));
  } else {
    // Resizing failed, just go with a bigger area.
    address = ReserveRegion(request_size);
    if (address == NULL) return;
  }
  address_ = address;
  size_ = request_size;
}


VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    bool result = ReleaseRegion(address_, size_);
    DCHECK(result);
    USE(result);
  }
}


bool VirtualMemory::IsReserved() {
  return address_ != NULL;
}


void VirtualMemory::Reset() {
  address_ = NULL;
  size_ = 0;
}


bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  return CommitRegion(address, size, is_executable);
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  DCHECK(IsReserved());
  return UncommitRegion(address, size);
}


void* VirtualMemory::ReserveRegion(size_t size) {
  return RandomizedVirtualAlloc(size, MEM_RESERVE, PAGE_NOACCESS);
}


bool VirtualMemory::CommitRegion(void* base, size_t size, bool is_executable) {
  int prot = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
  if (NULL == VirtualAlloc(base, size, MEM_COMMIT, prot)) {
    return false;
  }
  return true;
}


bool VirtualMemory::Guard(void* address) {
  if (NULL == VirtualAlloc(address,
                           OS::CommitPageSize(),
                           MEM_COMMIT,
                           PAGE_NOACCESS)) {
    return false;
  }
  return true;
}


bool VirtualMemory::UncommitRegion(void* base, size_t size) {
  return VirtualFree(base, size, MEM_DECOMMIT) != 0;
}


bool VirtualMemory::ReleaseRegion(void* base, size_t size) {
  return VirtualFree(base, 0, MEM_RELEASE) != 0;
}


bool VirtualMemory::HasLazyCommits() {
  // TODO(alph): implement for the platform.
  return false;
}

} }  // namespace v8::base
