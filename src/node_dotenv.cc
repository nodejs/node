#include "node_dotenv.h"
#include <regex>  // NOLINT(build/c++11)
#include <unordered_set>
#include "env-inl.h"
#include "node_file.h"
#include "uv.h"

namespace node {

using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;

/**
 * The inspiration for this implementation comes from the original dotenv code,
 * available at https://github.com/motdotla/dotenv
 */
const std::regex LINE(
    "\\s*(?:export\\s+)?([\\w.-]+)(?:\\s*=\\s*?|:\\s+?)(\\s*'(?:\\\\'|[^']"
    ")*'|\\s*\"(?:\\\\\"|[^\"])*\"|\\s*`(?:\\\\`|[^`])*`|[^#\r\n]+)?\\s*(?"
    ":#.*)?");  // NOLINT(whitespace/line_length)

std::vector<std::string> Dotenv::GetPathFromArgs(
    const std::vector<std::string>& args) {
  const auto find_match = [](const std::string& arg) {
    const std::string_view flag = "--env-file";
    return strncmp(arg.c_str(), flag.data(), flag.size()) == 0;
  };
  std::vector<std::string> paths;
  auto path = std::find_if(args.begin(), args.end(), find_match);

  while (path != args.end()) {
    auto equal_char = path->find('=');

    if (equal_char != std::string::npos) {
      paths.push_back(path->substr(equal_char + 1));
    } else {
      auto next_path = std::next(path);

      if (next_path == args.end()) {
        return paths;
      }

      paths.push_back(*next_path);
    }

    path = std::find_if(++path, args.end(), find_match);
  }

  return paths;
}

void Dotenv::SetEnvironment(node::Environment* env) {
  if (store_.empty()) {
    return;
  }

  auto isolate = env->isolate();

  for (const auto& entry : store_) {
    auto key = entry.first;
    auto value = entry.second;

    auto existing = env->env_vars()->Get(key.data());

    if (existing.IsNothing()) {
      env->env_vars()->Set(
          isolate,
          v8::String::NewFromUtf8(
              isolate, key.data(), NewStringType::kNormal, key.size())
              .ToLocalChecked(),
          v8::String::NewFromUtf8(
              isolate, value.data(), NewStringType::kNormal, value.size())
              .ToLocalChecked());
    }
  }
}

Local<Object> Dotenv::ToObject(Environment* env) {
  Local<Object> result = Object::New(env->isolate());

  for (const auto& entry : store_) {
    auto key = entry.first;
    auto value = entry.second;

    result
        ->Set(
            env->context(),
            v8::String::NewFromUtf8(
                env->isolate(), key.data(), NewStringType::kNormal, key.size())
                .ToLocalChecked(),
            v8::String::NewFromUtf8(env->isolate(),
                                    value.data(),
                                    NewStringType::kNormal,
                                    value.size())
                .ToLocalChecked())
        .Check();
  }

  return result;
}

void Dotenv::ParseContent(const std::string_view content) {
  std::string lines = std::string(content);
  lines = std::regex_replace(lines, std::regex("\r\n?"), "\n");

  std::smatch match;
  while (std::regex_search(lines, match, LINE)) {
    const std::string key = match[1].str();

    // Default undefined or null to an empty string
    std::string value = match[2].str();

    // Remove leading whitespaces
    value.erase(0, value.find_first_not_of(" \t"));

    // Remove trailing whitespaces
    value.erase(value.find_last_not_of(" \t") + 1);

    const char maybeQuote = value.front();

    if (maybeQuote == '"') {
      value = std::regex_replace(value, std::regex("\\\\n"), "\n");
      value = std::regex_replace(value, std::regex("\\\\r"), "\r");
    }

    // Remove surrounding quotes
    value = trim_quotes(value);

    store_.insert_or_assign(std::string(key), value);
    lines = match.suffix();
  }
}

Dotenv::ParseResult Dotenv::ParsePath(const std::string_view path) {
  uv_fs_t req;
  auto defer_req_cleanup = OnScopeLeave([&req]() { uv_fs_req_cleanup(&req); });

  uv_file file = uv_fs_open(nullptr, &req, path.data(), 0, 438, nullptr);
  if (req.result < 0) {
    // req will be cleaned up by scope leave.
    return ParseResult::FileError;
  }
  uv_fs_req_cleanup(&req);

  auto defer_close = OnScopeLeave([file]() {
    uv_fs_t close_req;
    CHECK_EQ(0, uv_fs_close(nullptr, &close_req, file, nullptr));
    uv_fs_req_cleanup(&close_req);
  });

  std::string result{};
  char buffer[8192];
  uv_buf_t buf = uv_buf_init(buffer, sizeof(buffer));

  while (true) {
    auto r = uv_fs_read(nullptr, &req, file, &buf, 1, -1, nullptr);
    if (req.result < 0) {
      // req will be cleaned up by scope leave.
      return ParseResult::InvalidContent;
    }
    uv_fs_req_cleanup(&req);
    if (r <= 0) {
      break;
    }
    result.append(buf.base, r);
  }

  ParseContent(result);
  return ParseResult::Valid;
}

void Dotenv::AssignNodeOptionsIfAvailable(std::string* node_options) {
  auto match = store_.find("NODE_OPTIONS");

  if (match != store_.end()) {
    *node_options = match->second;
  }
}

std::string_view Dotenv::trim_quotes(std::string_view str) {
  static const std::unordered_set<char> quotes = {'"', '\'', '`'};
  if (str.size() >= 2 && quotes.count(str.front()) &&
      quotes.count(str.back())) {
    str = str.substr(1, str.size() - 2);
  }
  return str;
}

}  // namespace node
