#include "node_single_executable_application.h"
#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)

namespace node {
namespace single_executable_application {

#define MAGIC_HEADER "JSCODE"
#define VERSION_CHARS "00000001"
#define FLAG_CHARS "00000000"

#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
static int callback(struct dl_phdr_info* info, size_t size, void* data) {
  // look for the segment with the magic number
  for (int index = 0; index < info->dlpi_phnum; index++) {
    if (info->dlpi_phdr[index].p_type == PT_LOAD) {
      char* content = reinterpret_cast<char*>(info->dlpi_addr +
                                              info->dlpi_phdr[index].p_vaddr);
      if (strncmp(MAGIC_HEADER, content, strlen(MAGIC_HEADER)) == 0) {
        *(static_cast<char**>(data)) = content;
        break;
      }
    }
  }
  return 0;
}
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)

struct single_executable_replacement_args* CheckForSingleBinary(int argc,
                                                                char** argv) {
  struct single_executable_replacement_args* new_args =
      new single_executable_replacement_args;
  new_args->single_executable_application = false;

#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
  char* singleBinaryData = nullptr;
  dl_iterate_phdr(callback, static_cast<void*>(&singleBinaryData));
  if (singleBinaryData != nullptr) {
    wordexp_t parsedArgs;
    wordexp(&(singleBinaryData[strlen(MAGIC_HEADER) + strlen(VERSION_CHARS) +
                               strlen(FLAG_CHARS)]),
            &parsedArgs,
            WRDE_NOCMD);
    new_args->argc = 0;
    while (parsedArgs.we_wordv[new_args->argc] != nullptr) new_args->argc++;
    new_args->argc = new_args->argc + argc;
    new_args->argv = new char*[new_args->argc];
    new_args->argv[0] = argv[0];
    int index = 1;
    while (parsedArgs.we_wordv[index - 1] != nullptr) {
      new_args->argv[index] = parsedArgs.we_wordv[index - 1];
      index++;
    }
    for (int i = 1; i < argc; i++) {
      new_args->argv[index++] = argv[i];
    }

    new_args->single_executable_application = true;
  }
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
  return new_args;
}

}  // namespace single_executable_application
}  // namespace node
