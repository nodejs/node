#include "node_string.h"
#include "node/inspector/protocol/Protocol.h"
#include "node_util.h"
#include "simdutf.h"
#include "util-inl.h"

namespace node {
namespace inspector {
namespace protocol {
namespace StringUtil {

size_t kNotFound = std::string::npos;

// NOLINTNEXTLINE(runtime/references) V8 API requirement
void builderAppendQuotedString(StringBuilder& builder,
                               const std::string_view string) {
  builder.put('"');
  if (!string.empty()) {
    size_t expected_utf16_length =
        simdutf::utf16_length_from_utf8(string.data(), string.length());
    MaybeStackBuffer<char16_t> buffer(expected_utf16_length);
    size_t utf16_length = simdutf::convert_utf8_to_utf16(
        string.data(), string.length(), buffer.out());
    CHECK_EQ(expected_utf16_length, utf16_length);
    escapeWideStringForJSON(reinterpret_cast<const uint16_t*>(buffer.out()),
                            utf16_length,
                            &builder);
  }
  builder.put('"');
}

std::unique_ptr<Value> parseJSON(const std::string_view string) {
  if (string.empty())
    return nullptr;
  size_t expected_utf16_length =
      simdutf::utf16_length_from_utf8(string.data(), string.length());
  MaybeStackBuffer<char16_t> buffer(expected_utf16_length);
  size_t utf16_length = simdutf::convert_utf8_to_utf16(
      string.data(), string.length(), buffer.out());
  CHECK_EQ(expected_utf16_length, utf16_length);
  return parseJSONCharacters(reinterpret_cast<const uint16_t*>(buffer.out()),
                             utf16_length);
}

std::unique_ptr<Value> parseJSON(v8_inspector::StringView string) {
  if (string.length() == 0)
    return nullptr;
  if (string.is8Bit())
    return parseJSONCharacters(string.characters8(), string.length());
  return parseJSONCharacters(string.characters16(), string.length());
}

String StringViewToUtf8(v8_inspector::StringView view) {
  if (view.length() == 0)
    return "";
  if (view.is8Bit()) {
    return std::string(reinterpret_cast<const char*>(view.characters8()),
                       view.length());
  }
  const char16_t* source =
      reinterpret_cast<const char16_t*>(view.characters16());
  size_t expected_utf8_length =
      simdutf::utf8_length_from_utf16(source, view.length());
  MaybeStackBuffer<char> buffer(expected_utf8_length);
  size_t utf8_length =
      simdutf::convert_utf16_to_utf8(source, view.length(), buffer.out());
  CHECK_EQ(expected_utf8_length, utf8_length);
  return String(buffer.out(), utf8_length);
}

String fromDouble(double d) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());  // Ignore current locale
  stream << d;
  return stream.str();
}

double toDouble(const char* buffer, size_t length, bool* ok) {
  std::istringstream stream(std::string(buffer, length));
  stream.imbue(std::locale::classic());  // Ignore current locale
  double d;
  stream >> d;
  *ok = !stream.fail();
  return d;
}

std::unique_ptr<Value> parseMessage(const std::string_view message,
                                    bool binary) {
  if (binary) {
    return Value::parseBinary(
        reinterpret_cast<const uint8_t*>(message.data()),
        message.length());
  }
  return parseJSON(message);
}

ProtocolMessage jsonToMessage(String message) {
  return message;
}

ProtocolMessage binaryToMessage(std::vector<uint8_t> message) {
  return std::string(reinterpret_cast<const char*>(message.data()),
                     message.size());
}

String fromUTF8(const uint8_t* data, size_t length) {
  return std::string(reinterpret_cast<const char*>(data), length);
}

String fromUTF16(const uint16_t* data, size_t length) {
  auto casted_data = reinterpret_cast<const char16_t*>(data);
  size_t expected_utf8_length =
      simdutf::utf8_length_from_utf16(casted_data, length);
  MaybeStackBuffer<char> buffer(expected_utf8_length);
  size_t utf8_length =
      simdutf::convert_utf16_to_utf8(casted_data, length, buffer.out());
  CHECK_EQ(expected_utf8_length, utf8_length);
  return String(buffer.out(), utf8_length);
}

const uint8_t* CharactersUTF8(const std::string_view s) {
  return reinterpret_cast<const uint8_t*>(s.data());
}

size_t CharacterCount(const std::string_view s) {
  // TODO(@anonrig): Test to make sure CharacterCount returns correctly.
  return simdutf::utf32_length_from_utf8(s.data(), s.length());
}

}  // namespace StringUtil
}  // namespace protocol
}  // namespace inspector
}  // namespace node

