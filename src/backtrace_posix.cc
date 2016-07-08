#include "node.h"

#if defined(__linux__)
#include <features.h>
#endif

#if defined(__linux__) && !defined(__GLIBC__) || defined(_AIX)
#define HAVE_EXECINFO_H 0
#else
#define HAVE_EXECINFO_H 1
#endif

#if HAVE_EXECINFO_H
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <stdio.h>
#endif

namespace node {

void DumpBacktrace(FILE* fp) {
#if HAVE_EXECINFO_H
  void* frames[256];
  const int size = backtrace(frames, arraysize(frames));
  if (size <= 0) {
    return;
  }
  for (int i = 1; i < size; i += 1) {
    void* frame = frames[i];
    fprintf(fp, "%2d: ", i);
    Dl_info info;
    const bool have_info = dladdr(frame, &info);
    if (!have_info || info.dli_sname == nullptr) {
      fprintf(fp, "%p", frame);
    } else if (char* demangled = abi::__cxa_demangle(info.dli_sname, 0, 0, 0)) {
      fprintf(fp, "%s", demangled);
      free(demangled);
    } else {
      fprintf(fp, "%s", info.dli_sname);
    }
    if (have_info && info.dli_fname != nullptr) {
      fprintf(fp, " [%s]", info.dli_fname);
    }
    fprintf(fp, "\n");
  }
#endif  // HAVE_EXECINFO_H
}

}  // namespace node
