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
  stream.imbue(std::locale("C"));  // Ignore locale
  stream << d;
  return stream.str();
}

double toDouble(const char* buffer, size_t length, bool* ok) {
  std::istringstream stream(std::string(buffer, length));
  stream.imbue(std::locale("C"));  // Ignore locale
  double d;
  stream >> d;
  *ok = !stream.fail();
  return d;
}

}  // namespace StringUtil
}  // namespace protocol
}  // namespace inspector
}  // namespace node

