#include "src/node_options.h"
#include "src/util.h"

#include <iostream>
#include <ranges>
#include <string>
#include <vector>

extern "C" {

uint64_t uv_get_total_memory() {
  return 0;
}

uint64_t uv_get_constrained_memory() {
  return 0;
}
};

namespace node {

void Assert(const AssertionInfo& info) {
  fprintf(stderr,
          "\n"
          "  #  %s at %s\n"
          "  #  Assertion failed: %s\n\n",
          info.function ? info.function : "(unknown function)",
          info.file_line ? info.file_line : "(unknown source location)",
          info.message);

  fflush(stderr);

  abort();
}

}  // namespace node

int main(int argc, char** argv) {
  std::vector<std::string> args(argv, argv + argc);
  std::vector<std::string> exec_args;
  std::vector<std::string> errors;

  node::PerProcessOptions cli_options;

  cli_options.cmdline = args;

  if (const char* result = std::getenv("NODE_OPTIONS")) {
    std::string node_options(result);

    std::vector<std::string> env_argv =
        node::ParseNodeOptionsEnvVar(node_options, &errors);
    for (auto error : errors) {
      std::cout << "error: " << error << std::endl;
    }
    if (!errors.empty()) return 1;

    env_argv.insert(env_argv.begin(), args.at(0));

    {
      std::vector<std::string> v8_args;
      node::options_parser::Parse(&env_argv,
                                  nullptr,
                                  &v8_args,
                                  &cli_options,
                                  node::OptionEnvvarSettings::kAllowedInEnvvar,
                                  &errors);

      for (auto arg : v8_args | std::views::drop(1)) {
        std::cout << "v8_arg: " << arg << std::endl;
      }
    }
  }

  node::HandleEnvOptions(cli_options.per_isolate->per_env,
                         [](const char* name) {
                           const char* value = std::getenv(name);
                           if (value != nullptr) {
                             return std::string(value);
                           }
                           return std::string("");
                         });

  {
    std::vector<std::string> v8_args;
    node::options_parser::Parse(&args,
                                &exec_args,
                                &v8_args,
                                &cli_options,
                                node::OptionEnvvarSettings::kDisallowedInEnvvar,
                                &errors);

    for (auto arg : v8_args | std::views::drop(1)) {
      std::cout << "v8_arg: " << arg << std::endl;
    }
  }

  for (auto arg : args) {
    std::cout << "arg: " << arg << std::endl;
  }

  for (auto arg : exec_args) {
    std::cout << "exec_arg: " << arg << std::endl;
  }

  for (auto error : errors) {
    std::cout << "error: " << error << std::endl;
  }
}
