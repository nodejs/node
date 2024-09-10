#include <assert.h>
#include "executable_wrapper.h"
#include "node.h"

extern "C" int32_t test_main_cpp_api(int32_t argc, char* argv[]);
extern "C" int32_t test_main_node_api(int32_t argc, char* argv[]);
extern "C" int32_t test_main_modules_node_api(int32_t argc, char* argv[]);
extern "C" int32_t test_main_linked_modules_node_api(int32_t argc,
                                                     char* argv[]);
extern "C" int32_t test_main_concurrent_node_api(int32_t argc, char* argv[]);
extern "C" int32_t test_main_multi_env_node_api(int32_t argc, char* argv[]);
extern "C" int32_t test_main_multi_thread_node_api(int32_t argc, char* argv[]);
extern "C" int32_t test_main_preload_node_api(int32_t argc, char* argv[]);
extern "C" int32_t test_main_snapshot_node_api(int32_t argc, char* argv[]);

typedef int32_t (*main_callback)(int32_t argc, char* argv[]);

int32_t CallWithoutArg1(main_callback main, int32_t argc, char** argv) {
  for (int32_t i = 2; i < argc; i++) {
    argv[i - 1] = argv[i];
  }
  argv[--argc] = nullptr;
  return main(argc, argv);
}

NODE_MAIN(int32_t argc, node::argv_type raw_argv[]) {
  char** argv = nullptr;
  node::FixupMain(argc, raw_argv, &argv);

  if (argc > 1) {
    const char* arg1 = argv[1];
    if (strcmp(arg1, "cpp-api") == 0) {
      return CallWithoutArg1(test_main_cpp_api, argc, argv);
    } else if (strcmp(arg1, "node-api") == 0) {
      return CallWithoutArg1(test_main_node_api, argc, argv);
    } else if (strcmp(arg1, "modules-node-api") == 0) {
      return CallWithoutArg1(test_main_modules_node_api, argc, argv);
    } else if (strcmp(arg1, "linked-modules-node-api") == 0) {
      return CallWithoutArg1(test_main_linked_modules_node_api, argc, argv);
    } else if (strcmp(arg1, "concurrent-node-api") == 0) {
      return CallWithoutArg1(test_main_concurrent_node_api, argc, argv);
    } else if (strcmp(arg1, "multi-env-node-api") == 0) {
      return CallWithoutArg1(test_main_multi_env_node_api, argc, argv);
    } else if (strcmp(arg1, "multi-thread-node-api") == 0) {
      return CallWithoutArg1(test_main_multi_thread_node_api, argc, argv);
    } else if (strcmp(arg1, "preload-node-api") == 0) {
      return CallWithoutArg1(test_main_preload_node_api, argc, argv);
    } else if (strcmp(arg1, "snapshot-node-api") == 0) {
      return CallWithoutArg1(test_main_snapshot_node_api, argc, argv);
    }
  }
  return test_main_cpp_api(argc, argv);
}
