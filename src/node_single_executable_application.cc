#include "node_single_executable_application.h"
#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
#include <fstream>

#include "env-inl.h"

namespace node {
namespace single_executable_application {

const char* sea_fuse = "AE249F4D38193B9BEFE654DF3AFD7065:00";
constexpr const int fuse_sentinal_length = 33;

constexpr const char* magic_header = "NODEJSSEA";
constexpr const char* version_chars = "00000001";
constexpr const char* flag_chars = "00000000";
constexpr const int argc_offset =
    strlen(magic_header) + strlen(version_chars) + strlen(flag_chars);
constexpr const int argc_length = 8;

#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
static int callback(struct dl_phdr_info* info, size_t size, void* data) {
  // look for the segment with the magic number
  for (int index = 0; index < info->dlpi_phnum; index++) {
    if (info->dlpi_phdr[index].p_type == PT_LOAD) {
      char* content = reinterpret_cast<char*>(info->dlpi_addr +
                                              info->dlpi_phdr[index].p_vaddr);
      if (strncmp(magic_header, content, strlen(magic_header)) == 0) {
        *(static_cast<char**>(data)) = content;
        break;
      }
    }
  }
  return 0;
}
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)

bool CheckFuse(void) {
  return (strncmp(sea_fuse + fuse_sentinal_length,
                  "01",
                  fuse_sentinal_length + 2) == 0);
}

// from Jesec's version
// 4096 chars should be more than enough to deal with
// header + node options + script size
// but definitely not elegant to have this limit
constexpr const int sea_buf_size = 4096;
char sea_buf[sea_buf_size];
std::string GetExecutablePath() {
  char exec_path_buf[2 * PATH_MAX];
  size_t exec_path_len = sizeof(exec_path_buf);

  if (uv_exepath(exec_path_buf, &exec_path_len) == 0) {
    return std::string(exec_path_buf, exec_path_len);
  }

  return "";
}

char* SearchExecutableForSEAData() {
  std::string exec = GetExecutablePath();
  if (exec.empty()) {
    return nullptr;
  }

  auto f = new std::ifstream(exec);
  if (!f->is_open() || !f->good() || f->eof()) {
    delete f;
    return nullptr;
  }

  std::string needle;
  needle += magic_header;
  needle += version_chars;

  constexpr auto buf_size = 1 << 20;

  auto buf = new char[buf_size];
  auto buf_view = std::string_view(buf, buf_size);
  auto buf_pos = buf_view.npos;

  size_t f_pos = 0;

  // first read
  f->read(buf, buf_size);
  f_pos += f->gcount();
  buf_pos = buf_view.find(needle);
  if (buf_pos != buf_view.npos) {
    f_pos = f_pos - f->gcount() + buf_pos;

    f->clear();
    f->seekg(f_pos, std::ios::beg);

    delete[] buf;
    f->read(sea_buf, sea_buf_size);
    return (sea_buf);
  }

  // subsequent reads, moving window
  while (!f->eof()) {
    std::memcpy(buf, buf + buf_size - needle.size(), needle.size());
    f->read(buf + needle.size(), buf_size - needle.size());
    f_pos += f->gcount();
    buf_pos = buf_view.find(needle);
    if (buf_pos != buf_view.npos) {
      f_pos = f_pos - f->gcount() - needle.size() + buf_pos;

      f->clear();
      f->seekg(f_pos, std::ios::beg);

      delete[] buf;
      f->read(sea_buf, sea_buf_size);
      return (sea_buf);
    }
  }

  delete[] buf;
  delete f;
  return nullptr;
}

char* GetSEAData() {
  char* single_executable_data = nullptr;
#if defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)
  dl_iterate_phdr(callback, static_cast<void*>(&single_executable_data));
#endif  // defined(__POSIX__) && !defined(_AIX) && !defined(__APPLE__)

  if (single_executable_data == nullptr) {
    // no special segment so read binary instead
    single_executable_data = SearchExecutableForSEAData();
  }

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
          static_cast<char*>(&single_executable_data[argc_offset]),
          argc_length);
      int argument_count = std::stoi(argc_string, 0, 16);
      char* arguments = &(single_executable_data[argc_offset + argc_length]);

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
