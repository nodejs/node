#include "node_string.h"
#include "node/inspector/protocol/Protocol.h"

#include <unicode/unistr.h>

namespace node {
namespace inspector {
namespace protocol {
namespace StringUtil {

size_t kNotFound = std::string::npos;

// NOLINTNEXTLINE(runtime/references) V8 API requirement
void builderAppendQuotedString(StringBuilder& builder, const String& string) {
  builder.put('"');
  if (!string.empty()) {
    icu::UnicodeString utf16 = icu::UnicodeString::fromUTF8(
        icu::StringPiece(string.data(), string.length()));
    escapeWideStringForJSON(
        reinterpret_cast<const uint16_t*>(utf16.getBuffer()), utf16.length(),
        &builder);
  }
  builder.put('"');
}

std::unique_ptr<Value> parseJSON(const String& string) {
  if (string.empty())
    return nullptr;

  icu::UnicodeString utf16 =
      icu::UnicodeString::fromUTF8(icu::StringPiece(string.data(),
                                                    string.length()));
  return parseJSONCharacters(
      reinterpret_cast<const uint16_t*>(utf16.getBuffer()), utf16.length());
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
  const uint16_t* source = view.characters16();
  const UChar* unicodeSource = reinterpret_cast<const UChar*>(source);
  static_assert(sizeof(*source) == sizeof(*unicodeSource),
                "sizeof(*source) == sizeof(*unicodeSource)");

  size_t result_length = view.length() * sizeof(*source);
  std::string result(result_length, '\0');
  icu::UnicodeString utf16(unicodeSource, view.length());
  // ICU components for std::string compatibility are not enabled in build...
  bool done = false;
  while (!done) {
    icu::CheckedArrayByteSink sink(&result[0], result_length);
    utf16.toUTF8(sink);
    result_length = sink.NumberOfBytesAppended();
    result.resize(result_length);
    done = !sink.Overflowed();
  }
  return result;
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

std::unique_ptr<Value> parseMessage(const std::string& message, bool binary) {
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
  icu::UnicodeString utf16(reinterpret_cast<const char16_t*>(data), length);
  std::string result;
  return utf16.toUTF8String(result);
}

const uint8_t* CharactersUTF8(const String& s) {
  return reinterpret_cast<const uint8_t*>(s.data());
}

size_t CharacterCount(const String& s) {
  icu::UnicodeString utf16 =
      icu::UnicodeString::fromUTF8(icu::StringPiece(s.data(), s.length()));
  return utf16.countChar32();
}

}  // namespace StringUtil
}  // namespace protocol
}  // namespace inspector
}  // namespace node

