#include "node_single_executable_application.h"
#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)

namespace node {
namespace single_executable_application {

const char* sea_fuse = "AE249F4D38193B9BEFE654DF3AFD7065:00";
#define FUSE_SENTINAL_LENGTH 33

#define MAGIC_HEADER "NODEJSSEA"
#define VERSION_CHARS "00000001"
#define FLAG_CHARS "00000000"
#define ARGC_OFFSET                                                            \
  strlen(MAGIC_HEADER) + strlen(VERSION_CHARS) + strlen(FLAG_CHARS)
#define ARGC_LENGTH 8

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

bool CheckFuse(void) {
  return (strncmp(sea_fuse + FUSE_SENTINAL_LENGTH,
                  "01",
                  FUSE_SENTINAL_LENGTH + 2) == 0);
}

char* GetSEAData() {
  char* single_executable_data = nullptr;
#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
  dl_iterate_phdr(callback, static_cast<void*>(&single_executable_data));
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
  return single_executable_data;
}

bool CheckForSingleBinary(int argc,
                          char** argv,
                          std::vector<std::string>* new_argv) {
  bool single_executable_application = false;

  if (CheckFuse()) {
    char* single_executable_data = GetSEAData();
    if (single_executable_data != nullptr) {
      // get the new arguments info
      std::string argc_string(
          static_cast<char*>(&single_executable_data[ARGC_OFFSET]),
          ARGC_LENGTH);
      int argument_count = std::stoi(argc_string, 0, 16);
      char* arguments = &(single_executable_data[ARGC_OFFSET + ARGC_LENGTH]);

      // copy over the first argument which needs to stay in place
      new_argv->push_back(argv[0]);

      // copy over the new arguments
      for (int i = 0; i < argument_count; i++) {
        new_argv->push_back(arguments);
        int length = strlen(arguments);
        // TODO(mhdawson): add check that we don't overrun the segment
        arguments = arguments + length + 1;
      }

      // TODO(mhdawson): remaining data after arguments in binary data
      // that can be used by the single executable applicaiton.
      // Add a way for the application to get that data.

      // copy over the arguments passed when the executable was started
      for (int i = 1; i < argc; i++) {
        new_argv->push_back(argv[i]);
      }

      single_executable_application = true;
    }
  }
  return single_executable_application;
}

}  // namespace single_executable_application
}  // namespace node
