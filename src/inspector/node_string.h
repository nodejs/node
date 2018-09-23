// Bridges V8 Inspector generated code with the std::string used by the Node
// Compare to V8 counterpart - deps/v8/src/inspector/string-util.h
#ifndef SRC_INSPECTOR_NODE_STRING_H_
#define SRC_INSPECTOR_NODE_STRING_H_

#include "util.h"
#include "v8-inspector.h"

#include <cstring>
#include <sstream>
#include <string>

namespace node {
namespace inspector {
namespace protocol {

class Value;

using String = std::string;
using StringBuilder = std::ostringstream;

namespace StringUtil {
// NOLINTNEXTLINE(runtime/references) This is V8 API...
inline void builderAppend(StringBuilder& builder, char c) {
  builder.put(c);
}

// NOLINTNEXTLINE(runtime/references)
inline void builderAppend(StringBuilder& builder, const char* value,
                          size_t length) {
  builder.write(value, length);
}

// NOLINTNEXTLINE(runtime/references)
inline void builderAppend(StringBuilder& builder, const char* value) {
  builderAppend(builder, value, std::strlen(value));
}

// NOLINTNEXTLINE(runtime/references)
inline void builderAppend(StringBuilder& builder, const String& string) {
  builder << string;
}

// NOLINTNEXTLINE(runtime/references)
inline void builderReserve(StringBuilder& builder, size_t) {
  // ostringstream does not have a counterpart
}
inline String substring(const String& string, size_t start, size_t count) {
  return string.substr(start, count);
}
inline String fromInteger(int n) {
  return std::to_string(n);
}
inline String builderToString(const StringBuilder& builder) {
  return builder.str();
}
inline size_t find(const String& string, const char* substring) {
  return string.find(substring);
}
String fromDouble(double d);
double toDouble(const char* buffer, size_t length, bool* ok);

String StringViewToUtf8(v8_inspector::StringView view);

// NOLINTNEXTLINE(runtime/references)
void builderAppendQuotedString(StringBuilder& builder, const String&);
std::unique_ptr<Value> parseJSON(const String&);
std::unique_ptr<Value> parseJSON(v8_inspector::StringView view);

extern size_t kNotFound;
}  // namespace StringUtil
}  // namespace protocol
}  // namespace inspector
}  // namespace node

#define DCHECK CHECK
#define DCHECK_LT CHECK_LT

#endif  // SRC_INSPECTOR_NODE_STRING_H_
