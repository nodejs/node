#ifndef SRC_NODE_DOTENV_H_
#define SRC_NODE_DOTENV_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util-inl.h"
#include "v8.h"

#include <algorithm>
#include <map>
#include <optional>

namespace node {

class Dotenv {
 public:
  enum ParseResult { Valid, FileError, InvalidContent };
  struct env_file_data {
    std::string path;
    bool is_optional;
  };

  Dotenv() = default;
  Dotenv(const Dotenv& d) = delete;
  Dotenv(Dotenv&& d) noexcept = default;
  Dotenv& operator=(Dotenv&& d) noexcept = default;
  Dotenv& operator=(const Dotenv& d) = delete;
  ~Dotenv() = default;

  void ParseContent(const std::string_view content);
  ParseResult ParsePath(const std::string_view path);
  void AssignNodeOptionsIfAvailable(std::string* node_options) const;
  v8::Maybe<void> SetEnvironment(Environment* env);
  v8::MaybeLocal<v8::Object> ToObject(Environment* env) const;
  // The names of the variables parsed from the env files.
  std::vector<std::string> GetKeys() const;
  // The value of a variable parsed from the env files, if it is defined there.
  std::optional<std::string> Get(const std::string& key) const;

  static std::vector<env_file_data> GetDataFromArgs(
      const std::vector<std::string>& args);

  // For the --env-file store: keys are compared like environment variable
  // names, ignoring case on Windows.
  static Dotenv ForProcessEnvironment();

 private:
  struct EnvKeyLess {
    bool ignore_case = false;
    bool operator()(const std::string& a, const std::string& b) const {
      if (!ignore_case) return a < b;
      return std::lexicographical_compare(
          a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
            return ToLower(x) < ToLower(y);
          });
    }
  };

  explicit Dotenv(EnvKeyLess less) : store_(less) {}

  std::map<std::string, std::string, EnvKeyLess> store_;
};

namespace per_process {
// The env files passed with --env-file and --env-file-if-exists.
extern Dotenv dotenv_file;
}  // namespace per_process

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_DOTENV_H_
