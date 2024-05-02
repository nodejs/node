#ifndef SRC_NODE_TASK_RUNNER_H_
#define SRC_NODE_TASK_RUNNER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_internals.h"
#include "simdjson.h"
#include "spawn_sync.h"
#include "uv.h"

#include <optional>
#include <string_view>

namespace node {
namespace task_runner {

// ProcessRunner is the class responsible for running a process.
// A class instance is created for each process to be run.
// The class is responsible for spawning the process and handling its exit.
// The class also handles the environment variables and arguments.
class ProcessRunner {
 public:
  ProcessRunner(std::shared_ptr<InitializationResultImpl> result,
                std::string_view command_id,
                const std::optional<std::string>& positional_args);
  void Run();
  static void ExitCallback(uv_process_t* req,
                           int64_t exit_status,
                           int term_signal);

 private:
  uv_loop_t* loop_ = uv_default_loop();
  uv_process_t process_{};
  uv_process_options_t options_{};
  uv_stdio_container_t child_stdio[3]{};
  std::shared_ptr<InitializationResultImpl> init_result;
  std::vector<std::string> command_args_{};
  std::vector<std::string> env_vars_{};
  std::unique_ptr<char* []> env {};  // memory for options_.env
  std::unique_ptr<char* []> arg {};  // memory for options_.args

  // OnExit is the callback function that is called when the process exits.
  void OnExit(int64_t exit_status, int term_signal);

#ifdef _WIN32
  std::string file_ = "cmd.exe";
#else
  std::string file_ = "/bin/sh";
#endif  // _WIN32
};

void RunTask(std::shared_ptr<InitializationResultImpl> result,
             std::string_view command_id,
             const std::optional<std::string>& positional_args);
std::optional<std::string> GetPositionalArgs(
    const std::vector<std::string>& args);
std::string EscapeShell(const std::string& command);

}  // namespace task_runner
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_TASK_RUNNER_H_
