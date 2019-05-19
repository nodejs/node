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
using ProtocolMessage = std::string;

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

std::unique_ptr<Value> parseMessage(const std::string& message, bool binary);
ProtocolMessage jsonToMessage(String message);
ProtocolMessage binaryToMessage(std::vector<uint8_t> message);
String fromUTF8(const uint8_t* data, size_t length);
String fromUTF16(const uint16_t* data, size_t length);
const uint8_t* CharactersUTF8(const String& s);
size_t CharacterCount(const String& s);

// Unimplemented. The generated code will fall back to CharactersUTF8().
inline uint8_t* CharactersLatin1(const String& s) { return nullptr; }
inline const uint16_t* CharactersUTF16(const String& s) { return nullptr; }

extern size_t kNotFound;
}  // namespace StringUtil

// A read-only sequence of uninterpreted bytes with reference-counted storage.
// Though the templates for generating the protocol bindings reference
// this type, js_protocol.pdl doesn't have a field of type 'binary', so
// therefore it's unnecessary to provide an implementation here.
class Binary {
 public:
  const uint8_t* data() const { UNREACHABLE(); }
  size_t size() const { UNREACHABLE(); }
  String toBase64() const { UNREACHABLE(); }
  static Binary fromBase64(const String& base64, bool* success) {
    UNREACHABLE();
  }
  static Binary fromSpan(const uint8_t* data, size_t size) { UNREACHABLE(); }
};

}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NODE_STRING_H_
