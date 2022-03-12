#include "node_single_binary.h"
#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)

namespace node {
namespace single_binary {

#define MAGIC_HEADER "JSCODE"
#define VERSION_CHARS "00000001"
#define FLAG_CHARS "00000000"

#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
static int
callback(struct dl_phdr_info* info, size_t size, void* data) {
  // look for the segment with the magic number
  for (int index = 0; index < info->dlpi_phnum; index++) {
     if (info->dlpi_phdr[index].p_type == PT_LOAD) {
      char* content =
        reinterpret_cast<char*>(info->dlpi_addr +
                                info->dlpi_phdr[index].p_vaddr);
      if (strncmp(MAGIC_HEADER,
                  content,
                  strlen(MAGIC_HEADER)) == 0) {
        *(static_cast<char**>(data)) = content;
        break;
      }
    }
  }
  return 0;
}
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)

struct NewArgs* checkForSingleBinary(int argc, char** argv) {
  struct NewArgs* newArgs = new NewArgs;
  newArgs->singleBinary = false;

#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
  char* singleBinaryData = nullptr;
  dl_iterate_phdr(callback, static_cast<void*>(&singleBinaryData));
  if (singleBinaryData != nullptr) {
    wordexp_t parsedArgs;
    wordexp(&(singleBinaryData[strlen(MAGIC_HEADER) +
      strlen(VERSION_CHARS) +
      strlen(FLAG_CHARS)]),
      &parsedArgs, WRDE_NOCMD);
    newArgs->argc = 0;
    while (parsedArgs.we_wordv[newArgs->argc] != nullptr)
      newArgs->argc++;
    newArgs->argc = newArgs->argc + argc;
    newArgs->argv = new char*[newArgs->argc];
    newArgs->argv[0] = argv[0];
    int index = 1;
    while (parsedArgs.we_wordv[index-1] != nullptr) {
      newArgs->argv[index] = parsedArgs.we_wordv[index-1];
      index++;
    }
    for (int i = 1; i < argc; i++) {
      newArgs->argv[index++] = argv[i];
    }

    newArgs->singleBinary = true;
  }
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
  return newArgs;
}

}  // namespace single_binary
}  // namespace node
