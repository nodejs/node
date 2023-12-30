#ifndef SRC_PATH_H_
#define SRC_PATH_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string>
#include <vector>

namespace node {

class Environment;

bool IsPathSeparator(const char c) noexcept;

std::string NormalizeString(const std::string_view path,
                            bool allowAboveRoot,
                            const std::string_view separator);

std::string PathResolve(Environment* env,
                        const std::vector<std::string_view>& args);
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_PATH_H_
