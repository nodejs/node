#include "node_task_runner.h"
#include "util.h"

#include <filesystem>
#include <regex>  // NOLINT(build/c++11)

namespace node::task_runner {

#ifdef _WIN32
static constexpr const char* bin_path = "\\node_modules\\.bin";
#else
static constexpr const char* bin_path = "/node_modules/.bin";
#endif  // _WIN32

ProcessRunner::ProcessRunner(std::shared_ptr<InitializationResultImpl> result,
                             std::string_view package_json_path,
                             std::string_view script_name,
                             std::string_view command,
                             const PositionalArgs& positional_args) {
  memset(&options_, 0, sizeof(uv_process_options_t));

  // Get the current working directory.
  char cwd[PATH_MAX_BYTES];
  size_t cwd_size = PATH_MAX_BYTES;
  CHECK_EQ(uv_cwd(cwd, &cwd_size), 0);
  CHECK_GT(cwd_size, 0);

#ifdef _WIN32
  std::string current_bin_path = cwd + std::string(bin_path) + ";";
#else
  std::string current_bin_path = cwd + std::string(bin_path) + ":";
#endif  // _WIN32

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

#ifdef _WIN32
  options_.flags |= UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;
#endif

  init_result = std::move(result);

  // Set the process handle data to this class instance.
  // This is used to access the class instance from the OnExit callback.
  // It is required because libuv doesn't allow passing lambda functions as a
  // callback.
  process_.data = this;

  SetEnvironmentVariables(current_bin_path,
                          std::string_view(cwd, cwd_size),
                          package_json_path,
                          script_name);

  std::string command_str(command);

  // Use the stored reference on the instance.
  options_.file = file_.c_str();

  // Add positional arguments to the command string.
  // Note that each argument needs to be escaped.
  if (!positional_args.empty()) {
    for (const auto& arg : positional_args) {
      command_str += " " + EscapeShell(arg);
    }
  }

#ifdef _WIN32
  // We check whether file_ ends with cmd.exe in a case-insensitive manner.
  // C++20 provides ends_with, but we roll our own for compatibility.
  const char* cmdexe = "cmd.exe";
  if (file_.size() >= strlen(cmdexe) &&
      StringEqualNoCase(cmdexe,
                        file_.c_str() + file_.size() - strlen(cmdexe))) {
    // If the file is cmd.exe, use the following command line arguments:
    // "/c" Carries out the command and exit.
    // "/d" Disables execution of AutoRun commands.
    // "/s" Strip the first and last quotes (") around the <string> but leaves
    // the rest of the command unchanged.
    command_args_ = {
        options_.file, "/d", "/s", "/c", "\"" + command_str + "\""};
  } else {
    // If the file is not cmd.exe, and it is unclear wich shell is being used,
    // so assume -c is the correct syntax (Unix-like shells use -c for this
    // purpose).
    command_args_ = {options_.file, "-c", command_str};
  }
#else
  command_args_ = {options_.file, "-c", command_str};
#endif  // _WIN32

  auto argc = command_args_.size();
  CHECK_GE(argc, 1);
  arg = std::unique_ptr<char*[]>(new char*[argc + 1]);
  options_.args = arg.get();
  for (size_t i = 0; i < argc; ++i) {
    options_.args[i] = const_cast<char*>(command_args_[i].c_str());
  }
  options_.args[argc] = nullptr;
}

void ProcessRunner::SetEnvironmentVariables(const std::string& current_bin_path,
                                            std::string_view cwd,
                                            std::string_view package_json_path,
                                            std::string_view script_name) {
  uv_env_item_t* env_items;
  int env_count;
  CHECK_EQ(0, uv_os_environ(&env_items, &env_count));

  // Iterate over environment variables once to store them in the current
  // ProcessRunner instance.
  for (int i = 0; i < env_count; i++) {
    std::string name = env_items[i].name;
    std::string value = env_items[i].value;

#ifdef _WIN32
    // We use comspec environment variable to find cmd.exe path on Windows
    // Example: 'C:\\Windows\\system32\\cmd.exe'
    // If we don't find it, we fallback to 'cmd.exe' for Windows
    if (StringEqualNoCase(name.c_str(), "comspec")) {
      file_ = value;
    }
#endif  // _WIN32

    if (StringEqualNoCase(name.c_str(), "path")) {
      // Add bin_path to the beginning of the PATH
      value = current_bin_path + value;
    }
    env_vars_.push_back(name + "=" + value);
  }
  uv_os_free_environ(env_items, env_count);

  // Add NODE_RUN_SCRIPT_NAME environment variable to the environment
  // to indicate which script is being run.
  env_vars_.push_back("NODE_RUN_SCRIPT_NAME=" + std::string(script_name));

  // Add NODE_RUN_PACKAGE_JSON_PATH environment variable to the environment to
  // indicate which package.json is being processed.
  if (std::filesystem::path(package_json_path).is_absolute()) {
    // TODO(anonrig): Traverse up the directory tree until we find a
    // package.json
    env_vars_.push_back("NODE_RUN_PACKAGE_JSON_PATH=" +
                        std::string(package_json_path));
  } else {
    auto path = std::filesystem::path(cwd) / std::string(package_json_path);
    env_vars_.push_back("NODE_RUN_PACKAGE_JSON_PATH=" + path.string());
  }

  env = std::unique_ptr<char*[]>(new char*[env_vars_.size() + 1]);
  options_.env = env.get();
  for (size_t i = 0; i < env_vars_.size(); i++) {
    options_.env[i] = const_cast<char*>(env_vars_[i].c_str());
  }
  options_.env[env_vars_.size()] = nullptr;
}

// EscapeShell escapes a string to be used as a command line argument.
// Under Windows, we follow:
// https://daviddeley.com/autohotkey/parameters/parameters.htm
// Elsewhere:
// It replaces single quotes with "\\'" and double quotes with "\\\"".
// It also removes excessive quote pairs and handles edge cases.
std::string EscapeShell(const std::string_view input) {
  // If the input is an empty string, return a pair of quotes
  if (input.empty()) {
#ifdef _WIN32
    return "\"\"";
#else
    return "''";
#endif
  }

  static const std::string_view forbidden_characters =
      "[\t\n\r \"#$&'()*;<>?\\\\`|~]";

  // Check if input contains any forbidden characters
  // If it doesn't, return the input as is.
  if (input.find_first_of(forbidden_characters) == std::string::npos) {
    return std::string(input);
  }

  static const std::regex leadingQuotePairs("^(?:'')+(?!$)");

#ifdef _WIN32
  // Replace double quotes with single quotes and surround the string
  // with double quotes for Windows.
  std::string escaped =
      std::regex_replace(std::string(input), std::regex("\""), "\"\"");
  escaped = "\"" + escaped + "\"";
  // Remove excessive quote pairs and handle edge cases
  static const std::regex tripleSingleQuote("\\\\\"\"\"");
  escaped = std::regex_replace(escaped, leadingQuotePairs, "");
  escaped = std::regex_replace(escaped, tripleSingleQuote, "\\\"");
#else
  // Replace single quotes("'") with "\\'" and wrap the result
  // in single quotes.
  std::string escaped =
      std::regex_replace(std::string(input), std::regex("'"), "\\'");
  escaped = "'" + escaped + "'";
  // Remove excessive quote pairs and handle edge cases
  static const std::regex tripleSingleQuote("\\\\'''");
  escaped = std::regex_replace(escaped, leadingQuotePairs, "");
  escaped = std::regex_replace(escaped, tripleSingleQuote, "\\'");
#endif  // _WIN32

  return escaped;
}

// ExitCallback is the callback function that is called when the process exits.
// It closes the process handle and calls the OnExit function.
// It is defined as a static function due to the limitations of libuv.
void ProcessRunner::ExitCallback(uv_process_t* handle,
                                 int64_t exit_status,
                                 int term_signal) {
  auto self = reinterpret_cast<ProcessRunner*>(handle->data);
  uv_close(reinterpret_cast<uv_handle_t*>(handle), nullptr);
  self->OnExit(exit_status, term_signal);
}

void ProcessRunner::OnExit(int64_t exit_status, int term_signal) {
  if (exit_status > 0) {
    init_result->exit_code_ = ExitCode::kGenericUserError;
  } else {
    init_result->exit_code_ = ExitCode::kNoFailure;
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
             const std::vector<std::string_view>& positional_args) {
  std::string_view path = "package.json";
  std::string raw_json;

  // No need to exclude BOM since simdjson will skip it.
  if (ReadFileSync(&raw_json, path.data()) < 0) {
    fprintf(stderr, "Can't read package.json\n");
    result->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  simdjson::ondemand::parser json_parser;
  simdjson::ondemand::document document;
  simdjson::ondemand::object main_object;
  simdjson::error_code error = json_parser.iterate(raw_json).get(document);

  // If document is not an object, throw an error.
  if (error || document.get_object().get(main_object)) {
    fprintf(stderr, "Can't parse package.json\n");
    result->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  // If package_json object doesn't have "scripts" field, throw an error.
  simdjson::ondemand::object scripts_object;
  if (main_object["scripts"].get_object().get(scripts_object)) {
    fprintf(stderr, "Can't find \"scripts\" field in package.json\n");
    result->exit_code_ = ExitCode::kGenericUserError;
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
    result->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  auto runner =
      ProcessRunner(result, path, command_id, command, positional_args);
  runner.Run();
}

// GetPositionalArgs returns the positional arguments from the command line.
// If the "--" flag is not found, it returns an empty optional.
// Otherwise, it returns the positional arguments as a single string.
// Example: "node -- script.js arg1 arg2" returns "arg1 arg2".
PositionalArgs GetPositionalArgs(const std::vector<std::string>& args) {
  // If the "--" flag is not found, return an empty optional
  // Otherwise, return the positional arguments as a single string
  if (auto dash_dash = std::find(args.begin(), args.end(), "--");
      dash_dash != args.end()) {
    PositionalArgs positional_args{};
    for (auto it = dash_dash + 1; it != args.end(); ++it) {
      // SAFETY: The following code is safe because the lifetime of the
      // arguments is guaranteed to be valid until the end of the task runner.
      positional_args.push_back(std::string_view(it->c_str(), it->size()));
    }
    return positional_args;
  }

  return {};
}

}  // namespace node::task_runner
