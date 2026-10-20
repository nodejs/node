#ifndef SRC_PATH_H_
#define SRC_PATH_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string>
#include <vector>
#include "node_options-inl.h"
#include "util-inl.h"

namespace node {

#ifdef _WIN32
constexpr bool IsPathSeparator(const char c) noexcept {
  return c == '\\' || c == '/';
}
#else   // POSIX
constexpr bool IsPathSeparator(const char c) noexcept {
  return c == '/';
}
#endif  // _WIN32

// Not _WIN32-only: callers reason about windows-style paths from any host.
constexpr bool IsWindowsDeviceRoot(const char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

std::string NormalizeString(const std::string_view path,
                            bool allowAboveRoot,
                            const std::string_view separator);

std::string PathResolve(Environment* env,
                        const std::vector<std::string_view>& paths);
std::string NormalizeFileURLOrPath(Environment* env, std::string_view path);
bool IsAbsoluteFilePath(std::string_view path);

#ifdef _WIN32
constexpr bool IsWindowsDriveLetter(const std::string_view path) noexcept {
  return path.size() > 2 && IsWindowsDeviceRoot(path[0]) &&
         (path[1] == ':' && (path[2] == '/' || path[2] == '\\'));
}
#endif  // _WIN32

void ToNamespacedPath(Environment* env, BufferValue* path);
void FromNamespacedPath(std::string* path);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_PATH_H_
