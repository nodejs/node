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
    USE(SymInitialize(current_process_, nullptr, true));
  }

  ~Win32SymbolDebuggingContext() override {
    USE(SymCleanup(current_process_));
  }

  using NameAndDisplacement = std::pair<std::string, DWORD64>;
  NameAndDisplacement WrappedSymFromAddr(DWORD64 dwAddress) const {
    // Refs: https://docs.microsoft.com/en-us/windows/desktop/Debug/retrieving-symbol-information-by-address
    // Patches:
    // Use `fprintf(stderr, ` instead of `printf`
    // `sym.filename = pSymbol->Name` on success
    // `current_process_` instead of `hProcess.
    DWORD64 dwDisplacement = 0;
    // Patch: made into arg - DWORD64  dwAddress = SOME_ADDRESS;

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    const auto pSymbol = reinterpret_cast<PSYMBOL_INFO>(buffer);

    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    if (SymFromAddr(current_process_, dwAddress, &dwDisplacement, pSymbol)) {
      // SymFromAddr returned success
      return NameAndDisplacement(pSymbol->Name, dwDisplacement);
    } else {
      // SymFromAddr failed
      const DWORD error = GetLastError();  // "eat" the error anyway
#ifdef DEBUG
      fprintf(stderr, "SymFromAddr returned error : %lu\n", error);
#endif
    }
    // End MSDN code

    return NameAndDisplacement();
  }

  SymbolInfo WrappedGetLine(DWORD64 dwAddress) const {
    SymbolInfo sym{};

    // Refs: https://docs.microsoft.com/en-us/windows/desktop/Debug/retrieving-symbol-information-by-address
    // Patches:
    // Use `fprintf(stderr, ` instead of `printf`.
    // Assign values to `sym` on success.
    // `current_process_` instead of `hProcess.

    // Patch: made into arg - DWORD64  dwAddress;
    DWORD dwDisplacement;
    IMAGEHLP_LINE64 line;

    SymSetOptions(SYMOPT_LOAD_LINES);

    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    // Patch: made into arg - dwAddress = 0x1000000;

    if (SymGetLineFromAddr64(current_process_, dwAddress,
                             &dwDisplacement, &line)) {
      // SymGetLineFromAddr64 returned success
      sym.filename = line.FileName;
      sym.line = line.LineNumber;
    } else {
      // SymGetLineFromAddr64 failed
      const DWORD error = GetLastError();  // "eat" the error anyway
#ifdef DEBUG
      fprintf(stderr, "SymGetLineFromAddr64 returned error : %lu\n", error);
#endif
    }
    // End MSDN code

    return sym;
  }

  // Fills the SymbolInfo::name of the io/out argument `sym`
  std::string WrappedUnDecorateSymbolName(const char* name) const {
    // Refs: https://docs.microsoft.com/en-us/windows/desktop/Debug/retrieving-undecorated-symbol-names
    // Patches:
    // Use `fprintf(stderr, ` instead of `printf`.
    // return `szUndName` instead of `printf` on success
    char szUndName[MAX_SYM_NAME];
    if (UnDecorateSymbolName(name, szUndName, sizeof(szUndName),
                             UNDNAME_COMPLETE)) {
      // UnDecorateSymbolName returned success
      return szUndName;
    } else {
      // UnDecorateSymbolName failed
      const DWORD error = GetLastError();  // "eat" the error anyway
#ifdef DEBUG
      fprintf(stderr, "UnDecorateSymbolName returned error %lu\n", error);
#endif
    }
    return nullptr;
  }

  SymbolInfo LookupSymbol(void* address) override {
    const DWORD64 dw_address = reinterpret_cast<DWORD64>(address);
    SymbolInfo ret = WrappedGetLine(dw_address);
    std::tie(ret.name, ret.dis) = WrappedSymFromAddr(dw_address);
    if (!ret.name.empty()) {
      ret.name = WrappedUnDecorateSymbolName(ret.name.c_str());
    }
    return ret;
  }

  bool IsMapped(void* address) override {
    MEMORY_BASIC_INFORMATION info;

    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info))
      return false;

    return info.State == MEM_COMMIT && info.Protect != 0;
  }

  int GetStackTrace(void** frames, int count) override {
    return CaptureStackBackTrace(0, count, frames, nullptr);
  }

  Win32SymbolDebuggingContext(const Win32SymbolDebuggingContext&) = delete;
  Win32SymbolDebuggingContext(Win32SymbolDebuggingContext&&) = delete;
  Win32SymbolDebuggingContext operator=(const Win32SymbolDebuggingContext&)
    = delete;
  Win32SymbolDebuggingContext operator=(Win32SymbolDebuggingContext&&)
    = delete;

 private:
  HANDLE current_process_;
};

std::unique_ptr<NativeSymbolDebuggingContext>
NativeSymbolDebuggingContext::New() {
  return std::unique_ptr<NativeSymbolDebuggingContext>(
      new Win32SymbolDebuggingContext());
}

#endif  // __POSIX__

std::string NativeSymbolDebuggingContext::SymbolInfo::Display() const {
  std::ostringstream oss;
  oss << name;
  if (dis != 0) {
    oss << "+" << dis;
  }
  if (!filename.empty()) {
    oss << " [" << filename << ']';
  }
  if (line != 0) {
    oss << ":L" << line;
  }
  return oss.str();
}

void DumpBacktrace(FILE* fp) {
  auto sym_ctx = NativeSymbolDebuggingContext::New();
  void* frames[256];
  const int size = sym_ctx->GetStackTrace(frames, arraysize(frames));
  for (int i = 1; i < size; i += 1) {
    void* frame = frames[i];
    NativeSymbolDebuggingContext::SymbolInfo s = sym_ctx->LookupSymbol(frame);
    fprintf(fp, "%2d: %p %s\n", i, frame, s.Display().c_str());
  }
}

void CheckedUvLoopClose(uv_loop_t* loop) {
  if (uv_loop_close(loop) == 0) return;

  auto sym_ctx = NativeSymbolDebuggingContext::New();

  fprintf(stderr, "uv loop at [%p] has active handles\n", loop);

  uv_walk(loop, [](uv_handle_t* handle, void* arg) {
    auto sym_ctx = static_cast<NativeSymbolDebuggingContext*>(arg);

    fprintf(stderr, "[%p] %s\n", handle, uv_handle_type_name(handle->type));

    void* close_cb = reinterpret_cast<void*>(handle->close_cb);
    fprintf(stderr, "\tClose callback: %p %s\n",
        close_cb, sym_ctx->LookupSymbol(close_cb).Display().c_str());

    fprintf(stderr, "\tData: %p %s\n",
        handle->data, sym_ctx->LookupSymbol(handle->data).Display().c_str());

    // We are also interested in the first field of what `handle->data`
    // points to, because for C++ code that is usually the virtual table pointer
    // and gives us information about the exact kind of object we're looking at.
    void* first_field = nullptr;
    // `handle->data` might be any value, including `nullptr`, or something
    // cast from a completely different type; therefore, check that it’s
    // dereferencable first.
    if (sym_ctx->IsMapped(handle->data))
      first_field = *reinterpret_cast<void**>(handle->data);

    if (first_field != nullptr) {
      fprintf(stderr, "\t(First field): %p %s\n",
          first_field, sym_ctx->LookupSymbol(first_field).Display().c_str());
    }
  }, sym_ctx.get());

  fflush(stderr);
  // Finally, abort.
  CHECK(0 && "uv_loop_close() while having open handles");
}

}  // namespace node

extern "C" void __DumpBacktrace(FILE* fp) {
  node::DumpBacktrace(fp);
}
