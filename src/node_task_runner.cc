#include "node_task_runner.h"
#include "util-inl.h"

#include <regex>  // NOLINT(build/c++11)

namespace node::task_runner {

#ifdef _WIN32
static constexpr const char* env_var_separator = ";";
#else
static constexpr const char* env_var_separator = ":";
#endif  // _WIN32

ProcessRunner::ProcessRunner(std::shared_ptr<InitializationResultImpl> result,
                             const std::filesystem::path& package_json_path,
                             std::string_view script_name,
                             std::string_view command,
                             std::string_view path_env_var,
                             const PositionalArgs& positional_args)
    : init_result(std::move(result)),
      package_json_path_(package_json_path),
      script_name_(script_name),
      path_env_var_(path_env_var) {
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

#ifdef _WIN32
  options_.flags |= UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;
#endif

  // Set the process handle data to this class instance.
  // This is used to access the class instance from the OnExit callback.
  // It is required because libuv doesn't allow passing lambda functions as a
  // callback.
  process_.data = this;

  SetEnvironmentVariables();

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
  if (file_.ends_with("cmd.exe")) {
    // If the file is cmd.exe, use the following command line arguments:
    // "/c" Carries out the command and exit.
    // "/d" Disables execution of AutoRun commands.
    // "/s" Strip the first and last quotes (") around the <string> but leaves
    // the rest of the command unchanged.
    command_args_ = {
        options_.file, "/d", "/s", "/c", "\"" + command_str + "\""};
  } else {
    // If the file is not cmd.exe, and it is unclear which shell is being used,
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

void ProcessRunner::SetEnvironmentVariables() {
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
      // Add path env variable to the beginning of the PATH
      value = path_env_var_ + value;
    }
    env_vars_.push_back(name + "=" + value);
  }
  uv_os_free_environ(env_items, env_count);

  // Add NODE_RUN_SCRIPT_NAME environment variable to the environment
  // to indicate which script is being run.
  env_vars_.push_back("NODE_RUN_SCRIPT_NAME=" + script_name_);

  // Add NODE_RUN_PACKAGE_JSON_PATH environment variable to the environment to
  // indicate which package.json is being processed.
  env_vars_.push_back("NODE_RUN_PACKAGE_JSON_PATH=" +
                      package_json_path_.string());

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

  static constexpr std::string_view forbidden_characters =
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
  const auto self = static_cast<ProcessRunner*>(handle->data);
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
  // keeps the string alive until destructor
  cwd = package_json_path_.parent_path().string();
  options_.cwd = cwd.c_str();
  if (int r = uv_spawn(loop_, &process_, &options_)) {
    fprintf(stderr, "Error: %s\n", uv_strerror(r));
  }

  uv_run(loop_, UV_RUN_DEFAULT);
}

std::optional<std::tuple<std::filesystem::path, std::string, std::string>>
FindPackageJson(const std::filesystem::path& cwd) {
  auto package_json_path = cwd / "package.json";
  std::string raw_content;
  std::string path_env_var;
  auto root_path = cwd.root_path();

  for (auto directory_path = cwd;
       !std::filesystem::equivalent(root_path, directory_path);
       directory_path = directory_path.parent_path()) {
    // Append "path/node_modules/.bin" to the env var, if it is a directory.
    auto node_modules_bin = directory_path / "node_modules" / ".bin";
    if (std::filesystem::is_directory(node_modules_bin)) {
      path_env_var += node_modules_bin.string() + env_var_separator;
    }

    if (raw_content.empty()) {
      package_json_path = directory_path / "package.json";
      // This is required for Windows because std::filesystem::path::c_str()
      // returns wchar_t* on Windows, and char* on other platforms.
      std::string contents = package_json_path.string();
      USE(ReadFileSync(&raw_content, contents.c_str()) > 0);
    }
  }

  // This means that there is no package.json until the root directory.
  // In this case, we just return nullopt, which will terminate the process..
  if (raw_content.empty()) {
    return std::nullopt;
  }

  return {{package_json_path, raw_content, path_env_var}};
}

void RunTask(const std::shared_ptr<InitializationResultImpl>& result,
             std::string_view command_id,
             const std::vector<std::string_view>& positional_args) {
  auto cwd = std::filesystem::current_path();
  auto package_json = FindPackageJson(cwd);

  if (!package_json.has_value()) {
    fprintf(stderr,
            "Can't find package.json for directory %s\n",
            cwd.string().c_str());
    result->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  // - path: Path to the package.json file.
  // - raw_json: Raw content of the package.json file.
  // - path_env_var: This represents the `PATH` environment variable.
  //   It always ends with ";" or ":" depending on the platform.
  auto [path, raw_json, path_env_var] = *package_json;

  simdjson::ondemand::parser json_parser;
  simdjson::ondemand::document document;
  simdjson::ondemand::object main_object;

  if (json_parser.iterate(raw_json).get(document)) {
    fprintf(stderr, "Can't parse %s\n", path.string().c_str());
    result->exit_code_ = ExitCode::kGenericUserError;
    return;
  }
  // If document is not an object, throw an error.
  if (auto root_error = document.get_object().get(main_object)) {
    if (root_error == simdjson::error_code::INCORRECT_TYPE) {
      fprintf(stderr,
              "Root value unexpected not an object for %s\n\n",
              path.string().c_str());
    } else {
      fprintf(stderr, "Can't parse %s\n", path.string().c_str());
    }
    result->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  simdjson::ondemand::object scripts_object;
  bool have_scripts = main_object["scripts"].get_object().get(scripts_object) ==
                      simdjson::error_code::SUCCESS;

  std::string exec_cmd;
  if (have_scripts) {
    std::string_view cmd_string;
    auto err = scripts_object[command_id].get_string().get(cmd_string);
    if (err == simdjson::error_code::SUCCESS) {
      exec_cmd.assign(cmd_string);
      ProcessRunner runner(result,
                           path,
                           command_id,
                           exec_cmd,
                           path_env_var,
                           positional_args);
      runner.Run();
      return;
    }
    if (err == simdjson::error_code::INCORRECT_TYPE) {
      fprintf(stderr,
              "Script \"%.*s\" is unexpectedly not a string for %s\n\n",
              static_cast<int>(command_id.size()),
              command_id.data(),
              path.string().c_str());
      result->exit_code_ = ExitCode::kGenericUserError;
      return;
    }
  }

  // Try "bin"
  simdjson::ondemand::value bin_value;
  bool have_bin =
      main_object["bin"].get(bin_value) == simdjson::error_code::SUCCESS;

  if (!have_scripts && !have_bin) {
    fprintf(stderr,
            "Can't find \"scripts\" or \"bin\" fields in %s\n",
            path.string().c_str());
    result->exit_code_ = ExitCode::kGenericUserError;
    return;
  }

  if (have_bin) {
    simdjson::ondemand::json_type bin_type;
    if (!bin_value.type().get(bin_type)) {
      std::string exec_rel;

      if (bin_type == simdjson::ondemand::json_type::string) {
        // "bin": "./cli.js"
        std::string_view rel;
        if (!bin_value.get_string().get(rel)) {
          std::string_view pkg_name;
          if (!main_object["name"].get_string().get(pkg_name) &&
              pkg_name == command_id) {
            exec_rel.assign(rel);
          } else {
            fprintf(stderr, "Incorrect command for %s\n", path.string().c_str());
            result->exit_code_ = ExitCode::kGenericUserError;
            return;
          }
        }
      } else if (bin_type == simdjson::ondemand::json_type::object) {
        // "bin": { "foo": "./cli.js" }
        simdjson::ondemand::object bin_obj;
        if (bin_value.get_object().get(bin_obj) ==
            simdjson::error_code::SUCCESS) {
          std::string_view rel;
          auto err = bin_obj[command_id].get_string().get(rel);
          if (err == simdjson::error_code::SUCCESS) {
            exec_rel.assign(rel);
          } else if (err == simdjson::error_code::INCORRECT_TYPE) {
            fprintf(stderr,
                    "Bin \"%.*s\" is unexpectedly not a string for %s\n",
                    static_cast<int>(command_id.size()),
                    command_id.data(),
                    path.string().c_str());
            result->exit_code_ = ExitCode::kGenericUserError;
            return;
          }
        }
      } else {
        fprintf(stderr,
                "Bin \"%.*s\" is unexpectedly not a string for %s\n",
                static_cast<int>(command_id.size()),
                command_id.data(),
                path.string().c_str());
        result->exit_code_ = ExitCode::kGenericUserError;
        return;
      }

      if (!exec_rel.empty()) {
        std::filesystem::path exec_path(exec_rel);
        if (exec_path.is_relative()) exec_path = path.parent_path() / exec_path;

        auto ext = exec_path.extension().string();
        bool needs_node = ext == ".js" || ext == ".mjs" || ext == ".cjs";

        exec_cmd =
            needs_node ? "node " + EscapeShell(exec_path.string()) : exec_rel;

        ProcessRunner runner(
            result, path, command_id, exec_cmd, path_env_var, positional_args);
        runner.Run();
        return;
      }
    }
  }

  fprintf(stderr,
          "Unknown script or bin entry \"%.*s\" for %s\n\n",
          static_cast<int>(command_id.size()),
          command_id.data(),
          path.string().c_str());

  if (have_scripts) {
    fprintf(stderr, "Available scripts:\n");
    scripts_object.reset();
    simdjson::ondemand::value value;
    for (auto field : scripts_object) {
      std::string_view key_str, value_str;
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
  } else {
    fprintf(stderr, "No scripts defined in %s\n", path.string().c_str());
  }

  if (have_bin) {
    fprintf(stderr, "\nAvailable bins:\n");
    simdjson::ondemand::json_type t;
    if (!bin_value.type().get(t)) {
      if (t == simdjson::ondemand::json_type::string) {
        std::string_view rel, pkg_name;
        if (!bin_value.get_string().get(rel) &&
            !main_object["name"].get_string().get(pkg_name)) {
          fprintf(stderr,
                  "  %.*s: %.*s\n",
                  static_cast<int>(pkg_name.size()),
                  pkg_name.data(),
                  static_cast<int>(rel.size()),
                  rel.data());
        }
      } else if (t == simdjson::ondemand::json_type::object) {
        simdjson::ondemand::object bin_obj;
        if (!bin_value.get_object().get(bin_obj)) {
          bin_obj.reset();
          for (auto field : bin_obj) {
            std::string_view key_str, rel;
            if (!field.unescaped_key().get(key_str) &&
                !field.value().get_string().get(rel)) {
              fprintf(stderr,
                      "  %.*s: %.*s\n",
                      static_cast<int>(key_str.size()),
                      key_str.data(),
                      static_cast<int>(rel.size()),
                      rel.data());
            }
          }
        }
      }
    }
  } else {
    fprintf(stderr, "No bins defined in %s\n", path.string().c_str());
  }

  result->exit_code_ = ExitCode::kGenericUserError;
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
    positional_args.reserve(args.size() - (dash_dash - args.begin()));
    for (auto it = dash_dash + 1; it != args.end(); ++it) {
      // SAFETY: The following code is safe because the lifetime of the
      // arguments is guaranteed to be valid until the end of the task runner.
      positional_args.emplace_back(it->c_str(), it->size());
    }
    return positional_args;
  }

  return {};
}

}  // namespace node::task_runner
