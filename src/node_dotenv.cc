#include "node_dotenv.h"
#include <unordered_set>
#include "env-inl.h"
#include "node_file.h"
#include "uv.h"

namespace node {

using v8::EscapableHandleScope;
using v8::JustVoid;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::String;
using v8::Value;

std::vector<Dotenv::env_file_data> Dotenv::GetDataFromArgs(
    const std::vector<std::string>& args) {
  const std::string_view optional_env_file_flag = "--env-file-if-exists";

  const auto find_match = [](const std::string& arg) {
    return arg == "--" || arg == "--env-file" ||
           arg.starts_with("--env-file=") || arg == "--env-file-if-exists" ||
           arg.starts_with("--env-file-if-exists=");
  };

  std::vector<Dotenv::env_file_data> env_files;
  // This will be an iterator, pointing to args.end() if no matches are found
  auto matched_arg = std::find_if(args.begin(), args.end(), find_match);

  while (matched_arg != args.end()) {
    if (*matched_arg == "--") {
      return env_files;
    }

    auto equal_char_index = matched_arg->find('=');

    if (equal_char_index != std::string::npos) {
      // `--env-file=path`
      auto flag = matched_arg->substr(0, equal_char_index);
      auto file_path = matched_arg->substr(equal_char_index + 1);

      struct env_file_data env_file_data = {
          file_path, flag.starts_with(optional_env_file_flag)};
      env_files.push_back(env_file_data);
    } else {
      // `--env-file path`
      auto file_path = std::next(matched_arg);

      if (file_path == args.end()) {
        return env_files;
      }

      struct env_file_data env_file_data = {
          *file_path, matched_arg->starts_with(optional_env_file_flag)};
      env_files.push_back(env_file_data);
    }

    matched_arg = std::find_if(++matched_arg, args.end(), find_match);
  }

  return env_files;
}

Maybe<void> Dotenv::SetEnvironment(node::Environment* env) {
  Local<Value> name;
  Local<Value> val;
  auto context = env->context();

  for (const auto& entry : store_) {
    auto existing = env->env_vars()->Get(entry.first.data());
    if (!existing.has_value()) {
      if (!ToV8Value(context, entry.first).ToLocal(&name) ||
          !ToV8Value(context, entry.second).ToLocal(&val)) {
        return Nothing<void>();
      }
      env->env_vars()->Set(env->isolate(), name.As<String>(), val.As<String>());
    }
  }

  return JustVoid();
}

MaybeLocal<Object> Dotenv::ToObject(Environment* env) const {
  EscapableHandleScope scope(env->isolate());
  Local<Object> result = Object::New(env->isolate());

  Local<Value> name;
  Local<Value> val;
  auto context = env->context();

  for (const auto& entry : store_) {
    if (!ToV8Value(context, entry.first).ToLocal(&name) ||
        !ToV8Value(context, entry.second).ToLocal(&val) ||
        result->Set(context, name, val).IsNothing()) {
      return MaybeLocal<Object>();
    }
  }

  return scope.Escape(result);
}

// Removes space characters (spaces, tabs and newlines) from
// the start and end of a given input string
std::string_view trim_spaces(std::string_view input) {
  if (input.empty()) return "";

  auto pos_start = input.find_first_not_of(" \t\n");
  if (pos_start == std::string_view::npos) {
    return "";
  }

  auto pos_end = input.find_last_not_of(" \t\n");
  if (pos_end == std::string_view::npos) {
    return input.substr(pos_start);
  }

  return input.substr(pos_start, pos_end - pos_start + 1);
}

void Dotenv::ParseContent(const std::string_view input) {
  std::string lines(input);

  // Handle windows newlines "\r\n": remove "\r" and keep only "\n"
  lines.erase(std::remove(lines.begin(), lines.end(), '\r'), lines.end());

  std::string_view content = lines;
  content = trim_spaces(content);

  while (!content.empty()) {
    // Skip empty lines and comments
    if (content.front() == '\n' || content.front() == '#') {
      auto newline = content.find('\n');
      if (newline != std::string_view::npos) {
        content.remove_prefix(newline + 1);
      } else {
        content.remove_prefix(content.size());
      }
      continue;
    }

    auto equal = content.find('=');
    if (equal == std::string_view::npos) {
      // If the line does not contain an equal sign, key = value pair is invalid
      // Skip the line and move to the next one
      auto newline = content.find('\n');
      if (newline != std::string_view::npos) {
        content.remove_prefix(newline + 1);
      } else {
        content.remove_prefix(content.size());
      }
      continue;
    }

    std::string key = content.substr(0, equal);
    key = trim_spaces(key);

    content.remove_prefix(equal + 1);
    content = trim_spaces(content);

    if (key.empty() {
      auto newline = content.find('\n');
      if (newline != std::string_view::npos) {
        content.remove_prefix(newline + 1);
      } else {
        content.remove_prefix(content.size());
      }
      continue;
    })

    if (key.starts_with("export ")) {
      key.remove_prefix(7);
      key = trim_spaces(key);
      if (key.empty()) {
        auto newline = content.find('\n');
        if (newline != std::string_view::npos) {
          content.remove_prefix(newline + 1);
        } else {
          content.remove_prefix(content.size());
        }
        continue;
      }
    }

    if (content.empty() || content.front() == '\n') {
      store_.insert_or_assign(std::string(key), "");
      auto newline = content.find('\n');
      if (newline != std::string_view::npos) {
        content.remove_prefix(newline + 1);
      } else {
        content.remove_prefix(content.size());
      }
      continue;
    }

    bool did_store = false;

    if (content.front('"')) {
      auto closing_quote = content.find('"', 1);
      if (closing_quote != std::string_view::npos) {
        std::string_view raw_value = content.substr(1, closing_quote - 1);
        std::string expanded = std::string(raw_value);

        size_t pos = 0;
        while ((pos = expanded.find("\\n", pos)) != std::string_view::npos) {
          expanded.replace(pos, 2, "\n");
          pos += 1;
        }

        store_.insert_or_assign(std::string(key), expanded);
        did_store = true;

        auto newline_pos = content.find('\n', closing_quote + 1);
        if (newline_pos != std::string_view::npos) {
          content.remove_prefix(newline_pos + 1);
        } else {
          content.remove_prefix(content.size());
        }
      }
    }

    if (!did_store) {
      if (content.front() == '\'' ||
      content.front() == '`' ||
        content.front() == '"') {
        auto quote_char = content.front();
        auto closing_quote = content.find(content.front(), 1);
        if (closing_quote == std::string_view::npos) {
          auto newline = content.find('\n');
          std::string_view val;
          if (newline != std::string_view::npos) {
            val = content.substr(0, newline);
            store_.insert_or_assign(std::string(key), std::string(val));
            did_store = true;
            content.remove_prefix(newline + 1);
          } else {
            val = content;
            store_.insert_or_assign(std::string(key), std::string(val));
            did_store = true;
            content.remove_prefix(content.size());
          }
        } else {
          auto val = content.substr(1, closing_quote - 1);
          store_.insert_or_assign(std::string(key), std::string(val));
          did_store = true;
          auto newline = content.find('\n', closing_quote + 1);
          if (newline != std::string_view::npos) {
            content.remove_prefix(newline + 1);
          } else {
            content.remove_prefix(content.size());
          }
        }
      }
    }

    if (!did_store) {
      auto newline = content.find('\n');
      std::string_view val;
      if (newline != std::string_view::npos) {
        val = content.substr(0, newline);
        auto hash_pos = val.find('#');
        if (hash_pos != std::string_view::npos) {
          val = val.substr(0, hash_pos);
        }
        val = trim_spaces(val);
        store_.insert_or_assign(std::string(key), std::string(val));
        content.remove_prefix(newline);
      } else {
        val = content;
        store_.insert_or_assign(std::string(key), std::string(val));
        content.remove_prefix(content.size());
      }
    }
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

}  // namespace node
