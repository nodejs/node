#ifndef TOOLS_EXECUTABLE_WRAPPER_H_
#define TOOLS_EXECUTABLE_WRAPPER_H_

// TODO(joyeecheung): reuse this in mksnapshot.
#include "uv.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace node {
#ifdef _WIN32
using argv_type = wchar_t*;
#define NODE_MAIN int wmain

void FixupMain(int argc, argv_type raw_argv[], char*** argv) {
  // Convert argv to UTF8.
  *argv = new char*[argc + 1];
  for (int i = 0; i < argc; i++) {
    // Compute the size of the required buffer
    DWORD size = WideCharToMultiByte(
        CP_UTF8, 0, raw_argv[i], -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) {
      // This should never happen.
      fprintf(stderr, "Could not convert arguments to utf8.");
      exit(1);
    }
    // Do the actual conversion
    (*argv)[i] = new char[size];
    DWORD result = WideCharToMultiByte(
        CP_UTF8, 0, raw_argv[i], -1, (*argv)[i], size, nullptr, nullptr);
    if (result == 0) {
      // This should never happen.
      fprintf(stderr, "Could not convert arguments to utf8.");
      exit(1);
    }
  }
  (*argv)[argc] = nullptr;
}
#else

using argv_type = char*;
#define NODE_MAIN int main

void FixupMain(int argc, argv_type raw_argv[], char*** argv) {
  *argv = uv_setup_args(argc, raw_argv);
  // Disable stdio buffering, it interacts poorly with printf()
  // calls elsewhere in the program (e.g., any logging from V8.)
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
}
#endif

}  // namespace node

#endif  // TOOLS_EXECUTABLE_WRAPPER_H_
