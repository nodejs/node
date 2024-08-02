#include "node_dotenv.h"
#include <unordered_set>
#include "env-inl.h"
#include "node_file.h"
#include "uv.h"

namespace node {

using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;

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

Local<Object> Dotenv::ToObject(Environment* env) const {
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

std::string_view trim_quotes(std::string_view input) {
  if (input.empty()) return "";
  auto first = input.front();
  if ((first == '\'' || first == '"' || first == '`') &&
      input.back() == first) {
    input = input.substr(1, input.size() - 2);
  }

  return input;
}

std::string_view trim_spaces(std::string_view input) {
  if (input.empty()) return "";
  if (input.front() == ' ') {
    input.remove_prefix(input.find_first_not_of(' '));
  }
  if (!input.empty() && input.back() == ' ') {
    input = input.substr(0, input.find_last_not_of(' ') + 1);
  }
  return input;
}

std::string_view parse_key(std::string_view key) {
  key = trim_spaces(key);
  if (key.empty()) return key;

  if (key.starts_with("export ")) {
    key.remove_prefix(7);
  }
  return key;
}

std::string parse_value(std::string_view value) {
  value = trim_spaces(value);
  if (value.empty()) return "";

  auto trimmed = trim_quotes(value);
  if (value.front() == '\"' && value.back() == '\"') {
    // Expand \n to newline in double-quote strings
    size_t pos = 0;
    auto expanded = std::string(trimmed);
    while ((pos = expanded.find("\\n", pos)) != std::string_view::npos) {
      expanded.replace(pos, 2, "\n");
      pos += 1;
    }
    return expanded;
  } else {
    return std::string(trimmed);
  }
}

/**
 * Parse the content of a .env file.
 * We want to be compatible with motdotla/dotenv js package,
 * so some edge-cases might be handled differently than you expect.
 *
 * Check the test cases in test/cctest/test_dotenv.cc for more details.
 */
void Dotenv::ParseContent(const std::string_view input) {
  std::string_view key;
  std::string_view value;

  char quote = 0;
  bool inComment = false;
  std::string::size_type start = 0;
  std::string::size_type end = 0;

  for (std::string::size_type i = 0; i < input.size(); i++) {
    char c = input[i];
    // Finished parsing a new key
    if (!inComment && c == '=' && key.empty()) {
      key = parse_key(input.substr(start, i - start));
      while (i + 1 < input.size() && input[i + 1] == ' ') {
        // Skip whitespace after key
        i++;
      }
      start = i + 1;
      end = i + 1;
      continue;
    } else if (!inComment && (c == '"' || c == '\'' || c == '`')) {
      if (start == i) {
        quote = c;
      } else if (quote == c) {
        quote = 0;
      }

      end++;
    } else if (!inComment && c == '#' && quote == 0) {
      end = i;
      inComment = true;
    } else if ((c == '\n' || c == '\r') && quote == 0) {
      if (!key.empty()) {
        auto value_str = parse_value(input.substr(start, end - start));
        store_.insert_or_assign(std::string(key), value_str);
      }

      // Skip \n if it is a part of a \r
      if (i + 1 < input.size() && input[i + 1] == '\n') {
        i++;
      }
      start = i + 1;
      end = start;
      value = "";
      key = "";
      quote = 0;
      inComment = false;
    } else if (!inComment) {
      end++;
    }
  }

  if (!key.empty()) {
    auto value_str = parse_value(input.substr(start, end - start));
    store_.insert_or_assign(std::string(key), value_str);
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

void Dotenv::AssignNodeOptionsIfAvailable(std::string* node_options) const {
  auto match = store_.find("NODE_OPTIONS");

  if (match != store_.end()) {
    *node_options = match->second;
  }
}

std::optional<std::string> Dotenv::GetValue(const std::string_view key) const {
  auto match = store_.find(key.data());

  if (match != store_.end()) {
    return std::optional<std::string>{match->second};
  }
  return std::nullopt;
}

}  // namespace node
