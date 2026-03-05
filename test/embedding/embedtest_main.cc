#include <cstring>
#include <string_view>
#include <unordered_map>
#include "executable_wrapper.h"

int32_t test_main_cpp_api(int32_t argc, char* argv[]);

extern "C" int32_t test_main_c_api_nodejs_main(int32_t argc, char* argv[]);

using MainCallback = int32_t (*)(int32_t argc, char* argv[]);

int32_t CallWithoutArg1(MainCallback main, int32_t argc, char* argv[]) {
  for (int32_t i = 2; i < argc; i++) {
    argv[i - 1] = argv[i];
  }
  argv[--argc] = nullptr;
  return main(argc, argv);
}

NODE_MAIN(int32_t argc, node::argv_type raw_argv[]) {
  char** argv = nullptr;
  node::FixupMain(argc, raw_argv, &argv);

  const std::unordered_map<std::string_view, MainCallback> main_map = {
      {"cpp-api", test_main_cpp_api},
      {"c-api-nodejs-main", test_main_c_api_nodejs_main},
  };
  if (argc > 1) {
    char* arg1 = argv[1];
    for (const auto& [key, value] : main_map) {
      if (key == arg1) {
        return CallWithoutArg1(value, argc, argv);
      }
    }
  }
  return test_main_cpp_api(argc, argv);
}
