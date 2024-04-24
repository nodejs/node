#include "node_task_runner.h"
#include "util.h"

#include <regex>  // NOLINT(build/c++11)

namespace node::task_runner {

#ifdef _WIN32
static const char separator[] = "\\";
static const char bin_path[] = "node_modules\\.bin";
static const char task_file[] = "cmd.exe";
#else
static const char separator[] = "/";
static const char bin_path[] = "node_modules/.bin";
static const char task_file[] = "/bin/sh";
#endif  // _WIN32

ProcessRunner::ProcessRunner(
    std::shared_ptr<InitializationResultImpl> result,
    std::string_view command,
    const std::optional<std::string>& positional_args) {
  memset(&options_, 0, sizeof(uv_process_options_t));

  // Inherit stdin, stdout, and stderr from the parent process.
  options_.stdio_count = 3;
  child_stdio[0].flags = UV_INHERIT_FD;
  child_stdio[0].data.fd = 0;
  child_stdio[1].flags = UV_INHERIT_FD;
  child_stdio[1].data.fd = 1;
  child_stdio[2].flags = UV_INHERIT_FD;
  child_stdio[2].data.fd = 2;
  options_.stdio = child_stdio;
  options_.exit_cb = ExitCallback;
  options_.file = task_file;

  init_result = result;
  process_.data = this;

  // Get the current working directory.
  char cwd[PATH_MAX_BYTES];
  size_t cwd_size = PATH_MAX_BYTES;
  CHECK_EQ(uv_cwd(cwd, &cwd_size), 0);
  CHECK_GT(cwd_size, 0);

  // Example: "/Users/user/project/node_modules/.bin"
  std::string current_bin_path = cwd + std::string(separator) + bin_path;

  std::string command_str(command);
  // Conditionally append the positional arguments to the command string
  if (positional_args.has_value()) {
    // Properly escape the positional arguments
    command_str += " " + EscapeShell(positional_args.value());
  }

  // Example: "/bin/sh -c 'node test.js'"
  command_args_ = {options_.file, "-c", command_str};
  auto argc = command_args_.size();
  CHECK_GE(argc, 1);
  options_.args = new char*[argc + 1];
  for (size_t i = 0; i < argc; ++i) {
    options_.args[i] = const_cast<char*>(command_args_[i].c_str());
  }
  options_.args[argc] = nullptr;

  // Set environment variables
  uv_env_item_t* env_items;
  int env_count;
  int r = uv_os_environ(&env_items, &env_count);
  CHECK_EQ(r, 0);
  options_.env =
      reinterpret_cast<char**>(malloc(sizeof(char*) * (env_count + 1)));

  // Iterate over environment variables once to store them in the current
  // ProcessRunner instance
  for (int i = 0; i < env_count; i++) {
    std::string name = env_items[i].name;
    std::string value = env_items[i].value;
    // Check if environment variable key is matching case-insensitive "path"
    if (name.size() == 4 && StringEqualNoCaseN(name.c_str(), "path", 4)) {
      value.insert(0, current_bin_path + ":");
    }
    // Environment variables should be in "KEY=value" format
    value.insert(0, name + "=");
    env_vars_.push_back(value);
  }
  uv_os_free_environ(env_items, env_count);

  for (int i = 0; i < env_count; i++) {
    options_.env[i] = const_cast<char*>(env_vars_[i].c_str());
  }
  options_.env[env_count] = nullptr;
}

std::string EscapeShell(const std::string& input) {
  // If the input is an empty string, return a pair of quotes
  if (input.empty()) {
    return "''";
  }

  static const std::string_view forbidden_characters =
      "[\t\n\r \"#$&'()*;<>?\\\\`|~]";

  // Check if input contains any forbidden characters
  // If it doesn't, return the input as is.
  if (input.find_first_of(forbidden_characters) == std::string::npos) {
    return input;
  }

  // Replace single quotes("'") with "\\'"
  std::string escaped = std::regex_replace(input, std::regex("'"), "\\'");

  // Wrap the result in single quotes
  escaped = "'" + escaped + "'";

  // Remove excessive quote pairs and handle edge cases
  static const std::regex leadingQuotePairs("^(?:'')+(?!$)");
  static const std::regex tripleSingleQuote("\\\\'''");

  escaped = std::regex_replace(escaped, leadingQuotePairs, "");
  escaped = std::regex_replace(escaped, tripleSingleQuote, "\\'");

  return escaped;
}

void ProcessRunner::ExitCallback(uv_process_t* handle,
                                 int64_t exit_status,
                                 int term_signal) {
  auto self = reinterpret_cast<ProcessRunner*>(handle->data);
  uv_close(reinterpret_cast<uv_handle_t*>(handle), nullptr);
  self->OnExit(exit_status, term_signal);
}

void ProcessRunner::OnExit(int64_t exit_status, int term_signal) {
  if (exit_status > 0) {
    init_result.get()->exit_code_ = ExitCode::kGenericUserError;
  } else {
    init_result.get()->exit_code_ = ExitCode::kNoFailure;
  }
}

void ProcessRunner::Run() {
  if (int r = uv_spawn(loop_, &process_, &options_)) {
    fprintf(stderr, "Error: %s\n", uv_strerror(r));
  }

  uv_run(loop_, UV_RUN_DEFAULT);
}

void RunTask(std::shared_ptr<InitializationResultImpl> result,
             std::string_view command_id,
             const std::optional<std::string>& positional_args) {
  std::string_view path = "package.json";
  std::string raw_json;

  // No need to exclude BOM since simdjson will skip it.
  if (ReadFileSync(&raw_json, path.data()) < 0) {
    fprintf(stderr, "Can't read package.json\n");
    result.get()->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  simdjson::ondemand::parser json_parser;
  simdjson::ondemand::document document;
  simdjson::ondemand::object main_object;
  simdjson::error_code error = json_parser.iterate(raw_json).get(document);

  // If document is not an object, throw an error.
  if (error || document.get_object().get(main_object)) {
    fprintf(stderr, "Can't parse package.json\n");
    result.get()->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  // If package_json object doesn't have "scripts" field, throw an error.
  simdjson::ondemand::object scripts_object;
  if (main_object["scripts"].get_object().get(scripts_object)) {
    fprintf(stderr, "Can't find \"scripts\" field in package.json\n");
    result.get()->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  // If the command_id is not found in the scripts object, throw an error.
  std::string_view command;
  if (scripts_object[command_id].get_string().get(command)) {
    fprintf(stderr,
            "Missing script: \"%.*s\"\n\n",
            static_cast<int>(command_id.size()),
            command_id.data());
    fprintf(stderr, "Available scripts are:\n");

    // Reset the object to iterate over it again
    scripts_object.reset();
    simdjson::ondemand::value value;
    for (auto field : scripts_object) {
      std::string_view key_str;
      std::string_view value_str;
      if (!field.unescaped_key().get(key_str) && !field.value().get(value) &&
          !value.get_string().get(value_str)) {
        fprintf(stderr,
                "  %.*s: %.*s\n",
                static_cast<int>(key_str.size()),
                key_str.data(),
                static_cast<int>(value_str.size()),
                value_str.data());
      }
    }
    result.get()->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  auto runner = ProcessRunner(result, command, positional_args);
  runner.Run();
}

std::optional<std::string> GetPositionalArgs(
    const std::vector<std::string>& args) {
  // If the "--" flag is not found, return an empty optional
  // Otherwise, return the positional arguments as a single string
  if (auto dash_dash = std::find(args.begin(), args.end(), "--");
      dash_dash != args.end()) {
    std::string positional_args;
    for (auto it = dash_dash + 1; it != args.end(); ++it) {
      positional_args += it->c_str();
    }
    return positional_args;
  }

  return std::nullopt;
}

}  // namespace node::task_runner
