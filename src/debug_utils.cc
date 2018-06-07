#include "debug_utils.h"
#include "node_internals.h"

#ifdef __POSIX__
#if defined(__linux__)
#include <features.h>
#endif

#if defined(__linux__) && !defined(__GLIBC__) || \
    defined(__UCLIBC__) || \
    defined(_AIX)
#define HAVE_EXECINFO_H 0
#else
#define HAVE_EXECINFO_H 1
#endif

#if HAVE_EXECINFO_H
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#endif

#else  // __POSIX__

#include <windows.h>
#include <dbghelp.h>

#endif  // __POSIX__

namespace node {

#ifdef __POSIX__
#if HAVE_EXECINFO_H
class PosixSymbolDebuggingContext final : public NativeSymbolDebuggingContext {
 public:
  PosixSymbolDebuggingContext() : pagesize_(getpagesize()) { }

  SymbolInfo LookupSymbol(void* address) override {
    Dl_info info;
    const bool have_info = dladdr(address, &info);
    SymbolInfo ret;
    if (!have_info)
      return ret;

    if (info.dli_sname != nullptr) {
      if (char* demangled = abi::__cxa_demangle(info.dli_sname, 0, 0, 0)) {
        ret.name = demangled;
        free(demangled);
      } else {
        ret.name = info.dli_sname;
      }
    }

    if (info.dli_fname != nullptr) {
      ret.filename = info.dli_fname;
    }

    return ret;
  }

  bool IsMapped(void* address) override {
    void* page_aligned = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(address) & ~(pagesize_ - 1));
    return msync(page_aligned, pagesize_, MS_ASYNC) == 0;
  }

  int GetStackTrace(void** frames, int count) override {
    return backtrace(frames, count);
  }

 private:
  uintptr_t pagesize_;
};

std::unique_ptr<NativeSymbolDebuggingContext>
NativeSymbolDebuggingContext::New() {
  return std::unique_ptr<NativeSymbolDebuggingContext>(
      new PosixSymbolDebuggingContext());
}

#else  // HAVE_EXECINFO_H

std::unique_ptr<NativeSymbolDebuggingContext>
NativeSymbolDebuggingContext::New() {
  return std::unique_ptr<NativeSymbolDebuggingContext>(
      new NativeSymbolDebuggingContext());
}

#endif  // HAVE_EXECINFO_H

#else  // __POSIX__

class Win32SymbolDebuggingContext final : public NativeSymbolDebuggingContext {
 public:
  Win32SymbolDebuggingContext() {
    current_process_ = GetCurrentProcess();
    USE(SymInitialize(process, nullptr, true));
  }

  ~Win32SymbolDebuggingContext() {
    USE(SymCleanup(process));
  }

  SymbolInfo LookupSymbol(void* address) override {
    // Ref: https://msdn.microsoft.com/en-en/library/windows/desktop/ms680578(v=vs.85).aspx
    char info_buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    SYMBOL_INFO* info = reinterpret_cast<SYMBOL_INFO*>(info_buf);
    char demangled[MAX_SYM_NAME];

    info->MaxNameLen = MAX_SYM_NAME;
    info->SizeOfStruct = sizeof(SYMBOL_INFO);

    SymbolInfo ret;
    const bool have_info = SymFromAddr(process,
                                       reinterpret_cast<DWORD64>(address),
                                       nullptr,
                                       info);
    if (have_info && strlen(info->Name) == 0) {
      if (UnDecorateSymbolName(info->Name,
                               demangled_,
                               sizeof(demangled_),
                               UNDNAME_COMPLETE)) {
        ret.name = demangled_;
      } else {
        ret.name = info->Name;
      }
    }

    return ret;
  }

  bool IsMapped(void* address) override {
    MEMORY_BASIC_INFORMATION info;

    if (VirtualQuery(address, &info, sizeof(info)) != info)
      return false;

    return info.State == MEM_COMMIT && info.Protect != 0;
  }

  int GetStackTrace(void** frames, int count) override {
    return CaptureStackBackTrace(0, count, frames, nullptr);
  }

 private:
  HANDLE current_process_;
};

NativeSymbolDebuggingContext::New() {
  return std::unique_ptr<NativeSymbolDebuggingContext>(
      new Win32SymbolDebuggingContext());
}

#endif  // __POSIX__

std::string NativeSymbolDebuggingContext::SymbolInfo::Display() const {
  std::string ret = name;
  if (!filename.empty()) {
    ret += " [";
    ret += filename;
    ret += ']';
  }
  return ret;
}

void DumpBacktrace(FILE* fp) {
  auto sym_ctx = NativeSymbolDebuggingContext::New();
  void* frames[256];
  const int size = sym_ctx->GetStackTrace(frames, arraysize(frames));
  for (int i = 1; i < size; i += 1) {
    void* frame = frames[i];
    fprintf(fp, "%2d: %p %s\n",
            i, frame, sym_ctx->LookupSymbol(frame).Display().c_str());
  }
}

}  // namespace node
