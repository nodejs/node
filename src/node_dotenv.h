#ifndef SRC_NODE_DOTENV_H_
#define SRC_NODE_DOTENV_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util-inl.h"

#include <map>

namespace node {

class Dotenv {
 public:
  Dotenv() = default;
  Dotenv(const Dotenv& d) = default;
  Dotenv(Dotenv&& d) noexcept = default;
  Dotenv& operator=(Dotenv&& d) noexcept = default;
  Dotenv& operator=(const Dotenv& d) = default;
  ~Dotenv() = default;

  void ParsePath(const std::string_view path);
  void AssignNodeOptionsIfAvailable(std::string* node_options);
  void SetEnvironment(Environment* env);

  static std::vector<std::string> GetPathFromArgs(
      const std::vector<std::string>& args);

 private:
  void ParseLine(const std::string_view line);
  std::map<std::string, std::string> store_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_DOTENV_H_
