/* auto-generated on 2026-03-11 12:53:21 -0400. Do not edit! */
#include "merve.h"

/* begin file src/parser.cpp */
#include <array>
#include <cstdint>
#include <limits>

#ifdef MERVE_USE_SIMDUTF
#include <simdutf.h>
#endif

namespace lexer {

// ============================================================================
// Compile-time lookup tables for character classification
// ============================================================================

// Hex digit lookup table: maps char -> hex value (0-15), or 255 if invalid
static constexpr std::array<uint8_t, 256> kHexTable = []() consteval {
  std::array<uint8_t, 256> table{};
  for (int i = 0; i < 256; ++i) table[i] = 255;
  for (int i = '0'; i <= '9'; ++i) table[i] = static_cast<uint8_t>(i - '0');
  for (int i = 'a'; i <= 'f'; ++i) table[i] = static_cast<uint8_t>(i - 'a' + 10);
  for (int i = 'A'; i <= 'F'; ++i) table[i] = static_cast<uint8_t>(i - 'A' + 10);
  return table;
}();

// Simple escape lookup table: maps escape char -> result char
// Uses 0xFF as "not a simple escape" marker since '\0' is a valid escape result
static constexpr std::array<uint8_t, 256> kSimpleEscapeTable = []() consteval {
  std::array<uint8_t, 256> table{};
  for (int i = 0; i < 256; ++i) table[i] = 0xFF;
  table['n'] = '\n';
  table['r'] = '\r';
  table['t'] = '\t';
  table['b'] = '\b';
  table['f'] = '\f';
  table['v'] = '\v';
  table['0'] = '\0';
  table['\\'] = '\\';
  table['\''] = '\'';
  table['"'] = '"';
  return table;
}();

// Punctuator lookup table
static constexpr std::array<bool, 256> kPunctuatorTable = []() consteval {
  std::array<bool, 256> table{};
  table['!'] = true;
  table['%'] = true;
  table['&'] = true;
  // ch > 39 && ch < 48: '(' ')' '*' '+' ',' '-' '.' '/'
  for (int i = 40; i < 48; ++i) table[i] = true;
  // ch > 57 && ch < 64: ':' ';' '<' '=' '>' '?'
  for (int i = 58; i < 64; ++i) table[i] = true;
  table['['] = true;
  table[']'] = true;
  table['^'] = true;
  // ch > 122 && ch < 127: '{' '|' '}' '~'
  for (int i = 123; i < 127; ++i) table[i] = true;
  return table;
}();

// Expression punctuator lookup table (similar but excludes ')' and '}')
static constexpr std::array<bool, 256> kExpressionPunctuatorTable = []() consteval {
  std::array<bool, 256> table{};
  table['!'] = true;
  table['%'] = true;
  table['&'] = true;
  // ch > 39 && ch < 47 && ch != 41: '(' '*' '+' ',' '-' '.'
  for (int i = 40; i < 47; ++i) {
    if (i != 41) table[i] = true;  // Skip ')'
  }
  // ch > 57 && ch < 64: ':' ';' '<' '=' '>' '?'
  for (int i = 58; i < 64; ++i) table[i] = true;
  table['['] = true;
  table['^'] = true;
  // ch > 122 && ch < 127 && ch != '}': '{' '|' '~'
  for (int i = 123; i < 127; ++i) {
    if (i != 125) table[i] = true;  // Skip '}'
  }
  return table;
}();

// Identifier start lookup table (a-z, A-Z, _, $, >= 0x80)
static constexpr std::array<bool, 256> kIdentifierStartTable = []() consteval {
  std::array<bool, 256> table{};
  for (int i = 'a'; i <= 'z'; ++i) table[i] = true;
  for (int i = 'A'; i <= 'Z'; ++i) table[i] = true;
  table['_'] = true;
  table['$'] = true;
  // UTF-8 continuation bytes and lead bytes (>= 0x80)
  for (int i = 0x80; i < 256; ++i) table[i] = true;
  return table;
}();

// Identifier char lookup table (identifier start + digits)
static constexpr std::array<bool, 256> kIdentifierCharTable = []() consteval {
  std::array<bool, 256> table{};
  for (int i = 'a'; i <= 'z'; ++i) table[i] = true;
  for (int i = 'A'; i <= 'Z'; ++i) table[i] = true;
  table['_'] = true;
  table['$'] = true;
  for (int i = 0x80; i < 256; ++i) table[i] = true;
  for (int i = '0'; i <= '9'; ++i) table[i] = true;
  return table;
}();

// Whitespace/line break lookup table
static constexpr std::array<bool, 256> kBrOrWsTable = []() consteval {
  std::array<bool, 256> table{};
  // c > 8 && c < 14: \t \n \v \f \r
  for (int i = 9; i < 14; ++i) table[i] = true;
  table[32] = true;  // space
  return table;
}();

// ============================================================================
// Inline functions using lookup tables
// ============================================================================

// Parse a hex digit, returns -1 if invalid
inline int hexDigit(unsigned char c) {
  uint8_t val = kHexTable[c];
  return val == 255 ? -1 : static_cast<int>(val);
}

// Encode a Unicode code point as UTF-8 into the output string
inline void encodeUtf8(std::string& out, uint32_t codepoint) {
#ifdef MERVE_USE_SIMDUTF
  // Use simdutf for optimized UTF-32 to UTF-8 conversion
  char buf[4];
  size_t len = simdutf::convert_utf32_to_utf8(
      reinterpret_cast<const char32_t*>(&codepoint), 1, buf);
  out.append(buf, len);
#else
  if (codepoint <= 0x7F) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0x10FFFF) {
    out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
#endif
}

// Unescape JavaScript string escape sequences
// Returns empty optional on invalid escape sequences (like lone surrogates)
std::optional<std::string> unescapeJsString(std::string_view str) {
  std::string result;
  result.reserve(str.size());

  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] != '\\') {
      result.push_back(str[i]);
      continue;
    }

    if (++i >= str.size()) {
      return std::nullopt;  // Trailing backslash
    }

    // Check simple escape table first (single character escapes)
    uint8_t simple = kSimpleEscapeTable[static_cast<unsigned char>(str[i])];
    if (simple != 0xFF) {
      result.push_back(static_cast<char>(simple));
      continue;
    }

    // Handle complex escapes
    switch (str[i]) {
      case 'x': {
        // \xHH - two hex digits
        if (i + 2 >= str.size()) return std::nullopt;
        int h1 = hexDigit(static_cast<unsigned char>(str[i + 1]));
        int h2 = hexDigit(static_cast<unsigned char>(str[i + 2]));
        if (h1 < 0 || h2 < 0) return std::nullopt;
        result.push_back(static_cast<char>((h1 << 4) | h2));
        i += 2;
        break;
      }
      case 'u': {
        if (i + 1 >= str.size()) return std::nullopt;
        if (str[i + 1] == '{') {
          // \u{XXXX} - variable length hex
          size_t start = i + 2;
          size_t end_brace = str.find('}', start);
          if (end_brace == std::string_view::npos || end_brace == start) return std::nullopt;
          uint32_t codepoint = 0;
          for (size_t j = start; j < end_brace; ++j) {
            int digit = hexDigit(static_cast<unsigned char>(str[j]));
            if (digit < 0) return std::nullopt;
            codepoint = (codepoint << 4) | static_cast<uint32_t>(digit);
            if (codepoint > 0x10FFFF) return std::nullopt;  // Invalid codepoint
          }
          // Handle surrogate pairs in \u{XXXX} format
          if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            // High surrogate - check for low surrogate \u{XXXX}
            if (end_brace + 3 < str.size() && str[end_brace + 1] == '\\' &&
                str[end_brace + 2] == 'u' && str[end_brace + 3] == '{') {
              size_t low_start = end_brace + 4;
              size_t low_end = str.find('}', low_start);
              if (low_end != std::string_view::npos && low_end > low_start) {
                uint32_t low = 0;
                bool valid_low = true;
                for (size_t j = low_start; j < low_end; ++j) {
                  int digit = hexDigit(static_cast<unsigned char>(str[j]));
                  if (digit < 0) {
                    valid_low = false;
                    break;
                  }
                  low = (low << 4) | static_cast<uint32_t>(digit);
                }
                if (valid_low && low >= 0xDC00 && low <= 0xDFFF) {
                  // Valid surrogate pair - combine into single codepoint
                  codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                  end_brace = low_end;  // Skip past the low surrogate
                } else {
                  // Lone high surrogate
                  return std::nullopt;
                }
              } else {
                // Lone high surrogate
                return std::nullopt;
              }
            } else {
              // Lone high surrogate
              return std::nullopt;
            }
          } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
            // Lone low surrogate
            return std::nullopt;
          }
          encodeUtf8(result, codepoint);
          i = end_brace;
        } else {
          // \uHHHH - exactly four hex digits
          if (i + 4 >= str.size()) return std::nullopt;
          uint32_t codepoint = 0;
          for (int j = 1; j <= 4; ++j) {
            int digit = hexDigit(static_cast<unsigned char>(str[i + static_cast<size_t>(j)]));
            if (digit < 0) return std::nullopt;
            codepoint = (codepoint << 4) | static_cast<uint32_t>(digit);
          }
          // Handle surrogate pairs
          if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            // High surrogate - check for low surrogate
            if (i + 10 < str.size() && str[i + 5] == '\\' && str[i + 6] == 'u') {
              uint32_t low = 0;
              bool valid_low = true;
              for (int j = 7; j <= 10; ++j) {
                int digit = hexDigit(static_cast<unsigned char>(str[i + static_cast<size_t>(j)]));
                if (digit < 0) {
                  valid_low = false;
                  break;
                }
                low = (low << 4) | static_cast<uint32_t>(digit);
              }
              if (valid_low && low >= 0xDC00 && low <= 0xDFFF) {
                // Valid surrogate pair - combine into single codepoint
                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                i += 6;  // Skip the low surrogate
              } else {
                // Lone high surrogate
                return std::nullopt;
              }
            } else {
              // Lone high surrogate
              return std::nullopt;
            }
          } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
            // Lone low surrogate
            return std::nullopt;
          }
          encodeUtf8(result, codepoint);
          i += 4;
        }
        break;
      }
      default:
        // Unknown escape - just include the character as-is
        result.push_back(str[i]);
        break;
    }
  }

  return result;
}

// Stack depth limits
constexpr size_t STACK_DEPTH = 2048;
constexpr size_t MAX_STAR_EXPORTS = 256;

// RequireType enum for parsing require statements
enum class RequireType {
  Import,
  ExportAssign,
  ExportStar
};

// StarExportBinding structure for tracking star export bindings
struct StarExportBinding {
  std::string_view specifier;
  std::string_view id;
};

// Thread-local state for error tracking (safe for concurrent parse calls).
thread_local std::optional<lexer_error> last_error;
thread_local std::optional<error_location> last_error_location;

static error_location makeErrorLocation(const char* source, const char* end, const char* at) {
  const char* target = at;
  if (target < source) target = source;
  if (target > end) target = end;

  uint32_t line = 1;
  uint32_t column = 1;
  const char* cur = source;

  while (cur < target) {
    const char ch = *cur++;
    if (ch == '\n') {
      line++;
      column = 1;
      continue;
    }
    if (ch == '\r') {
      line++;
      column = 1;
      if (cur < target && *cur == '\n') {
        cur++;
      }
      continue;
    }
    column++;
  }

  error_location loc{};
  loc.line = line;
  loc.column = column;
  return loc;
}

// Lexer state class
class CJSLexer {
private:
  const char* source;
  const char* pos;
  const char* end;
  const char* lastTokenPos;

  uint16_t templateStackDepth;
  uint16_t openTokenDepth;
  uint16_t templateDepth;

  uint32_t line;

  bool lastSlashWasDivision;
  bool nextBraceIsClass;

  std::array<uint16_t, STACK_DEPTH> templateStack_;
  std::array<const char*, STACK_DEPTH> openTokenPosStack_;
  std::array<char, STACK_DEPTH> openTokenTypeStack_;
  std::array<bool, STACK_DEPTH> openClassPosStack;
  std::array<StarExportBinding, MAX_STAR_EXPORTS> starExportStack_;
  StarExportBinding* starExportStack;
  const StarExportBinding* STAR_EXPORT_STACK_END;

  std::vector<export_entry>& exports;
  std::vector<export_entry>& re_exports;

  // Increments `line` when consuming a line terminator.
  // - Counts '\n' as a newline.
  // - Counts '\r' as a newline only when it is not part of a CRLF sequence.
  //   (i.e., the next character is not '\n' or we're at end-of-input.)
  void countNewline(char ch) {
    line += (ch == '\n') || (ch == '\r' && (pos + 1 >= end || *(pos + 1) != '\n'));
  }

  // Character classification helpers using lookup tables
  static bool isBr(char c) {
    return c == '\r' || c == '\n';
  }

  static bool isBrOrWs(unsigned char c) {
    return kBrOrWsTable[c];
  }

  static bool isBrOrWsOrPunctuatorNotDot(unsigned char c) {
    return kBrOrWsTable[c] || (kPunctuatorTable[c] && c != '.');
  }

  static bool isPunctuator(unsigned char ch) {
    return kPunctuatorTable[ch];
  }

  static bool isExpressionPunctuator(unsigned char ch) {
    return kExpressionPunctuatorTable[ch];
  }

  // String comparison helpers using string_view for cleaner, more maintainable code
  static constexpr bool matchesAt(const char* p, const char* end_pos, std::string_view expected) {
    size_t available = static_cast<size_t>(end_pos - p);
    if (available < expected.size()) return false;
    for (size_t i = 0; i < expected.size(); ++i) {
      if (p[i] != expected[i]) return false;
    }
    return true;
  }

  // Character type detection using lookup tables
  static bool isIdentifierStart(uint8_t ch) {
    return kIdentifierStartTable[ch];
  }

  static bool isIdentifierChar(uint8_t ch) {
    return kIdentifierCharTable[ch];
  }

  constexpr bool keywordStart(const char* p) const {
    return p == source || isBrOrWsOrPunctuatorNotDot(*(p - 1));
  }

  constexpr bool readPrecedingKeyword(const char* p, std::string_view keyword) const {
    if (p - static_cast<ptrdiff_t>(keyword.size()) + 1 < source) return false;
    const char* start = p - keyword.size() + 1;
    return matchesAt(start, end, keyword) && (start == source || isBrOrWsOrPunctuatorNotDot(*(start - 1)));
  }

  // Keyword detection
  constexpr bool isExpressionKeyword(const char* p) const {
    switch (*p) {
      case 'd':
        switch (*(p - 1)) {
          case 'i':
            return readPrecedingKeyword(p - 2, "vo");
          case 'l':
            return readPrecedingKeyword(p - 2, "yie");
          default:
            return false;
        }
      case 'e':
        switch (*(p - 1)) {
          case 's':
            switch (*(p - 2)) {
              case 'l':
                return p - 3 >= source && *(p - 3) == 'e' && keywordStart(p - 3);
              case 'a':
                return p - 3 >= source && *(p - 3) == 'c' && keywordStart(p - 3);
              default:
                return false;
            }
          case 't':
            return readPrecedingKeyword(p - 2, "dele");
          default:
            return false;
        }
      case 'f':
        if (*(p - 1) != 'o' || *(p - 2) != 'e')
          return false;
        switch (*(p - 3)) {
          case 'c':
            return readPrecedingKeyword(p - 4, "instan");
          case 'p':
            return readPrecedingKeyword(p - 4, "ty");
          default:
            return false;
        }
      case 'n':
        return (p - 1 >= source && *(p - 1) == 'i' && keywordStart(p - 1)) ||
               readPrecedingKeyword(p - 1, "retur");
      case 'o':
        return p - 1 >= source && *(p - 1) == 'd' && keywordStart(p - 1);
      case 'r':
        return readPrecedingKeyword(p - 1, "debugge");
      case 't':
        return readPrecedingKeyword(p - 1, "awai");
      case 'w':
        switch (*(p - 1)) {
          case 'e':
            return p - 2 >= source && *(p - 2) == 'n' && keywordStart(p - 2);
          case 'o':
            return readPrecedingKeyword(p - 2, "thr");
          default:
            return false;
        }
    }
    return false;
  }

  constexpr bool isParenKeyword(const char* curPos) const {
    return readPrecedingKeyword(curPos, "while") ||
           readPrecedingKeyword(curPos, "for") ||
           readPrecedingKeyword(curPos, "if");
  }

  constexpr bool isExpressionTerminator(const char* curPos) const {
    switch (*curPos) {
      case '>':
        return *(curPos - 1) == '=';
      case ';':
      case ')':
        return true;
      case 'h':
        return readPrecedingKeyword(curPos - 1, "catc");
      case 'y':
        return readPrecedingKeyword(curPos - 1, "finall");
      case 'e':
        return readPrecedingKeyword(curPos - 1, "els");
    }
    return false;
  }

  // Parsing utilities
  void syntaxError(lexer_error code, const char* at = nullptr) {
    if (!last_error) {
      last_error = code;
      const char* error_pos = at ? at : pos;
      last_error_location = makeErrorLocation(source, end, error_pos);
    }
    pos = end + 1;
  }

  char commentWhitespace() {
    char ch;
    do {
      if (pos >= end) return '\0';
      ch = *pos;
      if (ch == '/') {
        char next_ch = pos + 1 < end ? *(pos + 1) : '\0';
        if (next_ch == '/')
          lineComment();
        else if (next_ch == '*')
          blockComment();
        else
          return ch;
      } else if (!isBrOrWs(ch)) {
        return ch;
      } else {
        countNewline(ch);
      }
    } while (pos++ < end);
    return ch;
  }

  void lineComment() {
    while (pos++ < end) {
      char ch = *pos;
      if (ch == '\n' || ch == '\r') {
        countNewline(ch);
        return;
      }
    }
  }

  void blockComment() {
    pos++;
    while (pos++ < end) {
      char ch = *pos;
      if (ch == '*' && *(pos + 1) == '/') {
        pos++;
        return;
      }
      countNewline(ch);
    }
  }

  void stringLiteral(char quote) {
    while (pos++ < end) {
      char ch = *pos;
      if (ch == quote)
        return;
      if (ch == '\\') {
        if (pos + 1 >= end) break;
        ch = *++pos;
        if (ch == '\r') {
          ++line;
          if (*(pos + 1) == '\n')
            pos++;
        } else if (ch == '\n') {
          ++line;
        }
      } else if (isBr(ch))
        break;
    }
    syntaxError(lexer_error::UNTERMINATED_STRING_LITERAL);
  }

  void regularExpression() {
    while (pos++ < end) {
      char ch = *pos;
      if (ch == '/')
        return;
      if (ch == '[') {
        regexCharacterClass();
      } else if (ch == '\\') {
        if (pos + 1 < end)
          pos++;
      } else if (ch == '\n' || ch == '\r')
        break;
    }
    syntaxError(lexer_error::UNTERMINATED_REGEX);
  }

  void regexCharacterClass() {
    while (pos++ < end) {
      char ch = *pos;
      if (ch == ']')
        return;
      if (ch == '\\') {
        if (pos + 1 < end)
          pos++;
      } else if (ch == '\n' || ch == '\r')
        break;
    }
    syntaxError(lexer_error::UNTERMINATED_REGEX_CHARACTER_CLASS);
  }

  void templateString() {
    while (pos++ < end) {
      char ch = *pos;
      if (ch == '$' && *(pos + 1) == '{') {
        pos++;
        if (templateStackDepth >= STACK_DEPTH) {
          syntaxError(lexer_error::TEMPLATE_NEST_OVERFLOW);
          return;
        }
        templateStack_[templateStackDepth++] = templateDepth;
        templateDepth = ++openTokenDepth;
        return;
      }
      if (ch == '`')
        return;
      if (ch == '\\' && pos + 1 < end) {
        pos++;
        countNewline(*pos);
      } else {
        countNewline(ch);
      }
    }
    syntaxError(lexer_error::UNTERMINATED_TEMPLATE_STRING);
  }

  bool identifier(char startCh) {
    if (!isIdentifierStart(static_cast<uint8_t>(startCh)))
      return false;
    pos++;
    while (pos < end) {
      char ch = *pos;
      if (isIdentifierChar(static_cast<uint8_t>(ch))) {
        pos++;
      } else {
        break;
      }
    }
    return true;
  }

  // Check if string contains escape sequences
  static bool needsUnescaping(std::string_view str) {
#ifdef MERVE_USE_SIMDUTF
    // simdutf provides fast SIMD-based ASCII validation
    // If the string is valid ASCII without high bytes, we can use a faster path
    // But we still need to check for backslash
    const char* ptr = simdutf::find(str.data(), str.data() + str.size(), '\\');
    return ptr != str.data() + str.size();
#else
    return str.find('\\') != std::string_view::npos;
#endif
  }

  void addExport(std::string_view export_name, uint32_t at_line) {
    // Skip surrounding quotes if present
    if (!export_name.empty() && (export_name.front() == '\'' || export_name.front() == '"')) {
      export_name.remove_prefix(1);
      export_name.remove_suffix(1);
    }

    // Fast path: no escaping needed, use string_view directly
    if (!needsUnescaping(export_name)) {
      // Check if this export already exists (avoid duplicates)
      for (const auto& existing : exports) {
        if (get_string_view(existing.name) == export_name) {
          return; // Already exists, skip
        }
      }
      exports.push_back(export_entry{export_name, at_line});
      return;
    }

    // Slow path: unescape the export name (handles \u{XXXX}, \uHHHH, etc.)
    // Returns nullopt for invalid sequences like lone surrogates
    auto unescaped = unescapeJsString(export_name);
    if (!unescaped.has_value()) {
      return;  // Skip invalid escape sequences
    }

    const std::string& name = unescaped.value();

    // Check if this export already exists (avoid duplicates)
    for (const auto& existing : exports) {
      if (get_string_view(existing.name) == name) {
        return; // Already exists, skip
      }
    }
    exports.push_back(export_entry{std::move(unescaped.value()), at_line});
  }

  void addReexport(std::string_view reexport_name, uint32_t at_line) {
    // Skip surrounding quotes if present
    if (!reexport_name.empty() && (reexport_name.front() == '\'' || reexport_name.front() == '"')) {
      reexport_name.remove_prefix(1);
      reexport_name.remove_suffix(1);
    }

    // Fast path: no escaping needed, use string_view directly
    if (!needsUnescaping(reexport_name)) {
      re_exports.push_back(export_entry{reexport_name, at_line});
      return;
    }

    // Slow path: unescape the reexport name
    auto unescaped = unescapeJsString(reexport_name);
    if (!unescaped.has_value()) {
      return;  // Skip invalid escape sequences
    }

    re_exports.push_back(export_entry{std::move(unescaped.value()), at_line});
  }

  bool readExportsOrModuleDotExports(char ch) {
    const char* revertPos = pos;
    if (ch == 'm' && matchesAt(pos + 1, end, "odule")) {
      pos += 6;
      ch = commentWhitespace();
      if (ch != '.') {
        pos = revertPos;
        return false;
      }
      pos++;
      ch = commentWhitespace();
    }
    if (ch == 'e' && matchesAt(pos + 1, end, "xports")) {
      pos += 7;
      return true;
    }
    pos = revertPos;
    return false;
  }

  bool tryParseRequire(RequireType requireType) {
    const char* revertPos = pos;
    if (!matchesAt(pos + 1, end, "equire")) {
      return false;
    }
    pos += 7;
    char ch = commentWhitespace();
    if (ch == '(') {
      pos++;
      ch = commentWhitespace();
      const char* reexportStart = pos;
      if (ch == '\'' || ch == '"') {
        stringLiteral(ch);
        const char* reexportEnd = ++pos;
        ch = commentWhitespace();
        if (ch == ')') {
          switch (requireType) {
            case RequireType::ExportStar:
            case RequireType::ExportAssign:
              addReexport(std::string_view(reexportStart, reexportEnd - reexportStart), line);
              return true;
            default:
              if (starExportStack < STAR_EXPORT_STACK_END) {
                starExportStack->specifier = std::string_view(reexportStart, reexportEnd - reexportStart);
              }
              return true;
          }
        }
      }
    }
    pos = revertPos;
    return false;
  }

  // Helper to parse property value in object literal (identifier or require())
  bool tryParsePropertyValue(char& ch) {
    if (ch == 'r' && tryParseRequire(RequireType::ExportAssign)) {
      ch = *pos;
      return true;
    }
    if (identifier(ch)) {
      ch = *pos;
      return true;
    }
    return false;
  }

  void tryParseLiteralExports() {
    const char* revertPos = pos - 1;
    while (pos++ < end) {
      char ch = commentWhitespace();
      const char* startPos = pos;
      if (identifier(ch)) {
        const char* endPos = pos;
        ch = commentWhitespace();

        // Check if this is a getter syntax: get identifier() { ... }
        if (ch != ':' && endPos - startPos == 3 && matchesAt(startPos, end, "get") && identifier(ch)) {
          ch = commentWhitespace();
          if (ch == '(') {
            // This is a getter, stop parsing here (early termination)
            pos = revertPos;
            return;
          }
        }

        if (ch == ':') {
          pos++;
          ch = commentWhitespace();
          if (!tryParsePropertyValue(ch)) {
            pos = revertPos;
            return;
          }
        }
        addExport(std::string_view(startPos, endPos - startPos), line);
      } else if (ch == '\'' || ch == '"') {
        const char* start = pos;
        stringLiteral(ch);
        const char* end_pos = ++pos;
        ch = commentWhitespace();
        if (ch == ':') {
          pos++;
          ch = commentWhitespace();
          if (!tryParsePropertyValue(ch)) {
            pos = revertPos;
            return;
          }
          addExport(std::string_view(start, end_pos - start), line);
        }
      } else if (ch == '.' && matchesAt(pos + 1, end, "..")) {
        pos += 3;
        if (pos < end && *pos == 'r' && tryParseRequire(RequireType::ExportAssign)) {
          pos++;
        } else if (pos < end && !identifier(*pos)) {
          pos = revertPos;
          return;
        }
        ch = commentWhitespace();
      } else {
        pos = revertPos;
        return;
      }

      if (ch == '}')
        return;

      if (ch != ',') {
        pos = revertPos;
        return;
      }
    }
  }

  void tryParseExportsDotAssign(bool assign) {
    pos += 7;
    const char* revertPos = pos - 1;
    char ch = commentWhitespace();
    switch (ch) {
      case '.': {
        pos++;
        ch = commentWhitespace();
        const char* startPos = pos;
        if (identifier(ch)) {
          const char* endPos = pos;
          ch = commentWhitespace();
          if (ch == '=') {
            addExport(std::string_view(startPos, endPos - startPos), line);
            return;
          }
        }
        break;
      }
      case '[': {
        pos++;
        ch = commentWhitespace();
        if (ch == '\'' || ch == '"') {
          const char* startPos = pos;
          stringLiteral(ch);
          const char* endPos = ++pos;
          ch = commentWhitespace();
          if (ch != ']') break;
          pos++;
          ch = commentWhitespace();
          if (ch != '=') break;
          addExport(std::string_view(startPos, endPos - startPos), line);
        }
        break;
      }
      case '=': {
        if (assign) {
          re_exports.clear();
          pos++;
          ch = commentWhitespace();
          if (ch == '{') {
            tryParseLiteralExports();
            return;
          }
          if (ch == 'r')
            tryParseRequire(RequireType::ExportAssign);
        }
        break;
      }
    }
    pos = revertPos;
  }

  void tryParseModuleExportsDotAssign() {
    pos += 6;
    const char* revertPos = pos - 1;
    char ch = commentWhitespace();
    if (ch == '.') {
      pos++;
      ch = commentWhitespace();
      if (ch == 'e' && matchesAt(pos + 1, end, "xports")) {
        tryParseExportsDotAssign(true);
        return;
      }
    }
    pos = revertPos;
  }

  bool tryParseObjectHasOwnProperty(std::string_view it_id) {
    char ch = commentWhitespace();
    if (ch != 'O' || !matchesAt(pos + 1, end, "bject")) return false;
    pos += 6;
    ch = commentWhitespace();
    if (ch != '.') return false;
    pos++;
    ch = commentWhitespace();
    if (ch == 'p') {
      if (!matchesAt(pos + 1, end, "rototype")) return false;
      pos += 9;
      ch = commentWhitespace();
      if (ch != '.') return false;
      pos++;
      ch = commentWhitespace();
    }
    if (ch != 'h' || !matchesAt(pos + 1, end, "asOwnProperty")) return false;
    pos += 14;
    ch = commentWhitespace();
    if (ch != '.') return false;
    pos++;
    ch = commentWhitespace();
    if (ch != 'c' || !matchesAt(pos + 1, end, "all")) return false;
    pos += 4;
    ch = commentWhitespace();
    if (ch != '(') return false;
    pos++;
    ch = commentWhitespace();
    if (!identifier(ch)) return false;
    ch = commentWhitespace();
    if (ch != ',') return false;
    pos++;
    ch = commentWhitespace();
    if (!matchesAt(pos, end, it_id)) return false;
    pos += it_id.size();
    ch = commentWhitespace();
    if (ch != ')') return false;
    pos++;
    return true;
  }

  void tryParseObjectDefineOrKeys(bool keys) {
    pos += 6;
    const char* revertPos = pos - 1;
    char ch = commentWhitespace();
    if (ch == '.') {
      pos++;
      ch = commentWhitespace();
      if (ch == 'd' && matchesAt(pos + 1, end, "efineProperty")) {
        const char* exportStart = nullptr;
        const char* exportEnd = nullptr;
        while (true) {
          pos += 14;
          revertPos = pos - 1;
          ch = commentWhitespace();
          if (ch != '(') break;
          pos++;
          ch = commentWhitespace();
          if (!readExportsOrModuleDotExports(ch)) break;
          ch = commentWhitespace();
          if (ch != ',') break;
          pos++;
          ch = commentWhitespace();
          if (ch != '\'' && ch != '"') break;
          exportStart = pos;
          stringLiteral(ch);
          exportEnd = ++pos;
          ch = commentWhitespace();
          if (ch != ',') break;
          pos++;
          ch = commentWhitespace();
          if (ch != '{') break;
          pos++;
          ch = commentWhitespace();
          if (ch == 'e') {
            if (!matchesAt(pos + 1, end, "numerable")) break;
            pos += 10;
            ch = commentWhitespace();
            if (ch != ':') break;
            pos++;
            ch = commentWhitespace();
            if (ch != 't' || !matchesAt(pos + 1, end, "rue")) break;
            pos += 4;
            ch = commentWhitespace();
            if (ch != ',') break;
            pos++;
            ch = commentWhitespace();
          }
          if (ch == 'v') {
            if (!matchesAt(pos + 1, end, "alue")) break;
            pos += 5;
            ch = commentWhitespace();
            if (ch != ':') break;
            if (exportStart && exportEnd)
              addExport(std::string_view(exportStart, exportEnd - exportStart), line);
            pos = revertPos;
            return;
          } else if (ch == 'g') {
            if (!matchesAt(pos + 1, end, "et")) break;
            pos += 3;
            ch = commentWhitespace();
            if (ch == ':') {
              pos++;
              ch = commentWhitespace();
              if (ch != 'f') break;
              if (!matchesAt(pos + 1, end, "unction")) break;
              pos += 8;
              const char* lastPos = pos;
              ch = commentWhitespace();
              if (ch != '(' && (lastPos == pos || !identifier(ch))) break;
              ch = commentWhitespace();
            }
            if (ch != '(') break;
            pos++;
            ch = commentWhitespace();
            if (ch != ')') break;
            pos++;
            ch = commentWhitespace();
            if (ch != '{') break;
            pos++;
            ch = commentWhitespace();
            if (ch != 'r') break;
            if (!matchesAt(pos + 1, end, "eturn")) break;
            pos += 6;
            ch = commentWhitespace();
            if (!identifier(ch)) break;
            ch = commentWhitespace();
            if (ch == '.') {
              pos++;
              ch = commentWhitespace();
              if (!identifier(ch)) break;
              ch = commentWhitespace();
            } else if (ch == '[') {
              pos++;
              ch = commentWhitespace();
              if (ch == '\'' || ch == '"') {
                stringLiteral(ch);
              } else {
                break;
              }
              pos++;
              ch = commentWhitespace();
              if (ch != ']') break;
              pos++;
              ch = commentWhitespace();
            }
            if (ch == ';') {
              pos++;
              ch = commentWhitespace();
            }
            if (ch != '}') break;
            pos++;
            ch = commentWhitespace();
            if (ch == ',') {
              pos++;
              ch = commentWhitespace();
            }
            if (ch != '}') break;
            pos++;
            ch = commentWhitespace();
            if (ch != ')') break;
            if (exportStart && exportEnd)
              addExport(std::string_view(exportStart, exportEnd - exportStart), line);
            return;
          }
          break;
        }
      } else if (keys && ch == 'k' && matchesAt(pos + 1, end, "eys")) {
        while (true) {
          pos += 4;
          revertPos = pos - 1;
          ch = commentWhitespace();
          if (ch != '(') break;
          pos++;
          ch = commentWhitespace();
          const char* id_pos = pos;
          if (!identifier(ch)) break;
          std::string_view id(id_pos, static_cast<size_t>(pos - id_pos));
          ch = commentWhitespace();
          if (ch != ')') break;

          revertPos = pos++;
          ch = commentWhitespace();
          if (ch != '.') break;
          pos++;
          ch = commentWhitespace();
          if (ch != 'f' || !matchesAt(pos + 1, end, "orEach")) break;
          pos += 7;
          ch = commentWhitespace();
          revertPos = pos - 1;
          if (ch != '(') break;
          pos++;
          ch = commentWhitespace();
          if (ch != 'f' || !matchesAt(pos + 1, end, "unction")) break;
          pos += 8;
          ch = commentWhitespace();
          if (ch != '(') break;
          pos++;
          ch = commentWhitespace();
          const char* it_id_pos = pos;
          if (!identifier(ch)) break;
          std::string_view it_id(it_id_pos, static_cast<size_t>(pos - it_id_pos));
          ch = commentWhitespace();
          if (ch != ')') break;
          pos++;
          ch = commentWhitespace();
          if (ch != '{') break;
          pos++;
          ch = commentWhitespace();
          if (ch != 'i' || *(pos + 1) != 'f') break;
          pos += 2;
          ch = commentWhitespace();
          if (ch != '(') break;
          pos++;
          ch = commentWhitespace();
          if (!matchesAt(pos, end, it_id)) break;
          pos += it_id.size();
          ch = commentWhitespace();

          if (ch == '=') {
            if (!matchesAt(pos + 1, end, "==")) break;
            pos += 3;
            ch = commentWhitespace();
            if (ch != '"' && ch != '\'') break;
            char quot = ch;
            if (!matchesAt(pos + 1, end, "default")) break;
            pos += 8;
            ch = commentWhitespace();
            if (ch != quot) break;
            pos++;
            ch = commentWhitespace();
            if (ch != '|' || *(pos + 1) != '|') break;
            pos += 2;
            ch = commentWhitespace();
            if (!matchesAt(pos, end, it_id)) break;
            pos += it_id.size();
            ch = commentWhitespace();
            if (ch != '=' || !matchesAt(pos + 1, end, "==")) break;
            pos += 3;
            ch = commentWhitespace();
            if (ch != '"' && ch != '\'') break;
            quot = ch;
            if (!matchesAt(pos + 1, end, "__esModule")) break;
            pos += 11;
            ch = commentWhitespace();
            if (ch != quot) break;
            pos++;
            ch = commentWhitespace();
            if (ch != ')') break;
            pos++;
            ch = commentWhitespace();
            if (ch != 'r' || !matchesAt(pos + 1, end, "eturn")) break;
            pos += 6;
            ch = commentWhitespace();
            if (ch == ';')
              pos++;
            ch = commentWhitespace();

            if (ch == 'i' && *(pos + 1) == 'f') {
              bool inIf = true;
              pos += 2;
              ch = commentWhitespace();
              if (ch != '(') break;
              pos++;
              const char* ifInnerPos = pos;

              if (tryParseObjectHasOwnProperty(it_id)) {
                ch = commentWhitespace();
                if (ch != ')') break;
                pos++;
                ch = commentWhitespace();
                if (ch != 'r' || !matchesAt(pos + 1, end, "eturn")) break;
                pos += 6;
                ch = commentWhitespace();
                if (ch == ';')
                  pos++;
                ch = commentWhitespace();
                if (ch == 'i' && *(pos + 1) == 'f') {
                  pos += 2;
                  ch = commentWhitespace();
                  if (ch != '(') break;
                  pos++;
                } else {
                  inIf = false;
                }
              } else {
                pos = ifInnerPos;
              }

              if (inIf) {
                if (!matchesAt(pos, end, it_id)) break;
                pos += it_id.size();
                ch = commentWhitespace();
                if (ch != 'i' || !matchesAt(pos + 1, end, "n ")) break;
                pos += 3;
                ch = commentWhitespace();
                if (!readExportsOrModuleDotExports(ch)) break;
                ch = commentWhitespace();
                if (ch != '&' || *(pos + 1) != '&') break;
                pos += 2;
                ch = commentWhitespace();
                if (!readExportsOrModuleDotExports(ch)) break;
                ch = commentWhitespace();
                if (ch != '[') break;
                pos++;
                ch = commentWhitespace();
                if (!matchesAt(pos, end, it_id)) break;
                pos += it_id.size();
                ch = commentWhitespace();
                if (ch != ']') break;
                pos++;
                ch = commentWhitespace();
                if (ch != '=' || !matchesAt(pos + 1, end, "==")) break;
                pos += 3;
                ch = commentWhitespace();
                if (!matchesAt(pos, end, id)) break;
                pos += id.size();
                ch = commentWhitespace();
                if (ch != '[') break;
                pos++;
                ch = commentWhitespace();
                if (!matchesAt(pos, end, it_id)) break;
                pos += it_id.size();
                ch = commentWhitespace();
                if (ch != ']') break;
                pos++;
                ch = commentWhitespace();
                if (ch != ')') break;
                pos++;
                ch = commentWhitespace();
                if (ch != 'r' || !matchesAt(pos + 1, end, "eturn")) break;
                pos += 6;
                ch = commentWhitespace();
                if (ch == ';')
                  pos++;
                ch = commentWhitespace();
              }
            }
          } else if (ch == '!') {
            if (!matchesAt(pos + 1, end, "==")) break;
            pos += 3;
            ch = commentWhitespace();
            if (ch != '"' && ch != '\'') break;
            char quot = ch;
            if (!matchesAt(pos + 1, end, "default")) break;
            pos += 8;
            ch = commentWhitespace();
            if (ch != quot) break;
            pos++;
            ch = commentWhitespace();
            if (ch == '&') {
              if (*(pos + 1) != '&') break;
              pos += 2;
              ch = commentWhitespace();
              if (ch != '!') break;
              pos++;
              ch = commentWhitespace();
              if (ch == 'O' && matchesAt(pos + 1, end, "bject.")) {
                if (!tryParseObjectHasOwnProperty(it_id)) break;
              } else if (identifier(ch)) {
                ch = commentWhitespace();
                if (ch != '.') break;
                pos++;
                ch = commentWhitespace();
                if (ch != 'h' || !matchesAt(pos + 1, end, "asOwnProperty")) break;
                pos += 14;
                ch = commentWhitespace();
                if (ch != '(') break;
                pos++;
                ch = commentWhitespace();
                if (!matchesAt(pos, end, it_id)) break;
                pos += it_id.size();
                ch = commentWhitespace();
                if (ch != ')') break;
                pos++;
              }
              ch = commentWhitespace();
            }
            if (ch != ')') break;
            pos++;
            ch = commentWhitespace();
          } else {
            break;
          }

          if (readExportsOrModuleDotExports(ch)) {
            ch = commentWhitespace();
            if (ch != '[') break;
            pos++;
            ch = commentWhitespace();
            if (!matchesAt(pos, end, it_id)) break;
            pos += it_id.size();
            ch = commentWhitespace();
            if (ch != ']') break;
            pos++;
            ch = commentWhitespace();
            if (ch != '=') break;
            pos++;
            ch = commentWhitespace();
            if (!matchesAt(pos, end, id)) break;
            pos += id.size();
            ch = commentWhitespace();
            if (ch != '[') break;
            pos++;
            ch = commentWhitespace();
            if (!matchesAt(pos, end, it_id)) break;
            pos += it_id.size();
            ch = commentWhitespace();
            if (ch != ']') break;
            pos++;
            ch = commentWhitespace();
            if (ch == ';') {
              pos++;
              ch = commentWhitespace();
            }
          } else if (ch == 'O') {
            if (!matchesAt(pos + 1, end, "bject")) break;
            pos += 6;
            ch = commentWhitespace();
            if (ch != '.') break;
            pos++;
            ch = commentWhitespace();
            if (ch != 'd' || !matchesAt(pos + 1, end, "efineProperty")) break;
            pos += 14;
            ch = commentWhitespace();
            if (ch != '(') break;
            pos++;
            ch = commentWhitespace();
            if (!readExportsOrModuleDotExports(ch)) break;
            ch = commentWhitespace();
            if (ch != ',') break;
            pos++;
            ch = commentWhitespace();
            if (!matchesAt(pos, end, it_id)) break;
            pos += it_id.size();
            ch = commentWhitespace();
            if (ch != ',') break;
            pos++;
            ch = commentWhitespace();
            if (ch != '{') break;
            pos++;
            ch = commentWhitespace();
            if (ch != 'e' || !matchesAt(pos + 1, end, "numerable")) break;
            pos += 10;
            ch = commentWhitespace();
            if (ch != ':') break;
            pos++;
            ch = commentWhitespace();
            if (ch != 't' || !matchesAt(pos + 1, end, "rue")) break;
            pos += 4;
            ch = commentWhitespace();
            if (ch != ',') break;
            pos++;
            ch = commentWhitespace();
            if (ch != 'g' || !matchesAt(pos + 1, end, "et")) break;
            pos += 3;
            ch = commentWhitespace();
            if (ch == ':') {
              pos++;
              ch = commentWhitespace();
              if (ch != 'f') break;
              if (!matchesAt(pos + 1, end, "unction")) break;
              pos += 8;
              const char* lastPos = pos;
              ch = commentWhitespace();
              if (ch != '(' && (lastPos == pos || !identifier(ch))) break;
              ch = commentWhitespace();
            }
            if (ch != '(') break;
            pos++;
            ch = commentWhitespace();
            if (ch != ')') break;
            pos++;
            ch = commentWhitespace();
            if (ch != '{') break;
            pos++;
            ch = commentWhitespace();
            if (ch != 'r' || !matchesAt(pos + 1, end, "eturn")) break;
            pos += 6;
            ch = commentWhitespace();
            if (!matchesAt(pos, end, id)) break;
            pos += id.size();
            ch = commentWhitespace();
            if (ch != '[') break;
            pos++;
            ch = commentWhitespace();
            if (!matchesAt(pos, end, it_id)) break;
            pos += it_id.size();
            ch = commentWhitespace();
            if (ch != ']') break;
            pos++;
            ch = commentWhitespace();
            if (ch == ';') {
              pos++;
              ch = commentWhitespace();
            }
            if (ch != '}') break;
            pos++;
            ch = commentWhitespace();
            if (ch == ',') {
              pos++;
              ch = commentWhitespace();
            }
            if (ch != '}') break;
            pos++;
            ch = commentWhitespace();
            if (ch != ')') break;
            pos++;
            ch = commentWhitespace();
            if (ch == ';') {
              pos++;
              ch = commentWhitespace();
            }
          } else {
            break;
          }

          if (ch != '}') break;
          pos++;
          ch = commentWhitespace();
          if (ch != ')') break;

          // Search through export bindings to see if this is a star export
          StarExportBinding* curCheckBinding = &starExportStack_[0];
          while (curCheckBinding != starExportStack) {
            if (curCheckBinding->id == id) {
              addReexport(curCheckBinding->specifier, line);
              pos = revertPos;
              return;
            }
            curCheckBinding++;
          }
          return;
        }
      }
    }
    pos = revertPos;
  }

  void tryBacktrackAddStarExportBinding(const char* bPos) {
    if (bPos < source) return;
    while (bPos > source && *bPos == ' ')
      bPos--;
    if (*bPos == '=') {
      if (bPos <= source) return;
      bPos--;
      while (bPos > source && *bPos == ' ')
        bPos--;
      const char* id_end = bPos;
      bool identifierStart = false;
      while (bPos > source) {
        char ch = *bPos;
        if (!isIdentifierChar(static_cast<uint8_t>(ch)))
          break;
        identifierStart = isIdentifierStart(static_cast<uint8_t>(ch));
        bPos--;
      }
      if (identifierStart && *bPos == ' ') {
        if (starExportStack == STAR_EXPORT_STACK_END)
          return;
        starExportStack->id = std::string_view(bPos + 1, static_cast<size_t>(id_end - bPos));
        while (bPos > source && *bPos == ' ')
          bPos--;
        switch (*bPos) {
          case 'r':
            if (!readPrecedingKeyword(bPos - 1, "va"))
              return;
            break;
          case 't':
            if (!readPrecedingKeyword(bPos - 1, "le") && !readPrecedingKeyword(bPos - 1, "cons"))
              return;
            break;
          default:
            return;
        }
        starExportStack++;
      }
    }
  }

  void throwIfImportStatement() {
    const char* startPos = pos;
    pos += 6;
    char ch = commentWhitespace();
    switch (ch) {
      case '(':
        openTokenTypeStack_[openTokenDepth] = '(';
        openTokenPosStack_[openTokenDepth++] = startPos;
        return;
      case '.':
        // Check if followed by 'meta' (possibly with whitespace)
        pos++;
        ch = commentWhitespace();
        // Use str_eq4 for more efficient comparison
        if (ch == 'm' && pos + 4 <= end && matchesAt(pos + 1, end, "eta")) {
          // Check that 'meta' is not followed by an identifier character
          if (pos + 4 < end && isIdentifierChar(static_cast<uint8_t>(pos[4]))) {
            // It's something like import.metaData, not import.meta
            return;
          }
          syntaxError(lexer_error::UNEXPECTED_ESM_IMPORT_META, startPos);
        }
        return;
      default:
        if (pos == startPos + 6)
          break;
        [[fallthrough]];
      case '"':
      case '\'':
      case '{':
      case '*':
        if (openTokenDepth != 0) {
          pos--;
          return;
        }
        syntaxError(lexer_error::UNEXPECTED_ESM_IMPORT, startPos);
    }
  }

  void throwIfExportStatement() {
    const char* startPos = pos;
    pos += 6;
    const char* curPos = pos;
    char ch = commentWhitespace();
    if (pos == curPos && !isPunctuator(ch))
      return;
    syntaxError(lexer_error::UNEXPECTED_ESM_EXPORT, startPos);
  }

public:
  CJSLexer(std::vector<export_entry>& out_exports, std::vector<export_entry>& out_re_exports)
    : source(nullptr), pos(nullptr), end(nullptr), lastTokenPos(nullptr),
      templateStackDepth(0), openTokenDepth(0), templateDepth(0),
      line(1),
      lastSlashWasDivision(false), nextBraceIsClass(false),
      templateStack_{}, openTokenPosStack_{}, openTokenTypeStack_{}, openClassPosStack{},
      starExportStack_{}, starExportStack(nullptr), STAR_EXPORT_STACK_END(nullptr),
      exports(out_exports), re_exports(out_re_exports) {}

  bool parse(std::string_view file_contents) {
    source = file_contents.data();
    pos = source - 1;
    end = source + file_contents.size();
    // Initialize lastTokenPos to before source to detect start-of-input condition
    // when checking if '/' should be treated as regex vs division operator
    lastTokenPos = source - 1;

    templateStackDepth = 0;
    openTokenDepth = 0;
    templateDepth = std::numeric_limits<uint16_t>::max();
    line = 1;
    lastSlashWasDivision = false;
    starExportStack = &starExportStack_[0];
    STAR_EXPORT_STACK_END = &starExportStack_[MAX_STAR_EXPORTS - 1];
    nextBraceIsClass = false;

    char ch = '\0';

    // Handle shebang
    if (file_contents.size() >= 2 && source[0] == '#' && source[1] == '!') {
      if (file_contents.size() == 2)
        return true;
      pos += 2;
      while (pos < end) {
        ch = *pos;
        if (ch == '\n' || ch == '\r')
          break;
        pos++;
      }
      lastTokenPos = pos;  // Update lastTokenPos after shebang
    }

    while (pos++ < end) {
      ch = *pos;

      if (ch == ' ' || (ch < 14 && ch > 8)) {
        countNewline(ch);
        continue;
      }

      if (openTokenDepth == 0) {
        switch (ch) {
          case 'i':
            if (pos + 6 < end && matchesAt(pos + 1, end, "mport") && keywordStart(pos))
              throwIfImportStatement();
            lastTokenPos = pos;
            continue;
          case 'r': {
            const char* startPos = pos;
            if (tryParseRequire(RequireType::Import) && keywordStart(startPos))
              tryBacktrackAddStarExportBinding(startPos - 1);
            lastTokenPos = pos;
            continue;
          }
          case '_':
            if (pos + 23 < end && matchesAt(pos + 1, end, "interopRequireWildcard") && (keywordStart(pos) || *(pos - 1) == '.')) {
              const char* startPos = pos;
              pos += 23;
              if (*pos == '(') {
                pos++;
                openTokenTypeStack_[openTokenDepth] = '(';
                openTokenPosStack_[openTokenDepth++] = lastTokenPos;
                if (tryParseRequire(RequireType::Import) && keywordStart(startPos))
                  tryBacktrackAddStarExportBinding(startPos - 1);
              }
            } else if (pos + 8 < end && matchesAt(pos + 1, end, "_export") && (keywordStart(pos) || *(pos - 1) == '.')) {
              pos += 8;
              if (pos + 4 < end && matchesAt(pos, end, "Star"))
                pos += 4;
              if (*pos == '(') {
                openTokenTypeStack_[openTokenDepth] = '(';
                openTokenPosStack_[openTokenDepth++] = lastTokenPos;
                if (*(pos + 1) == 'r') {
                  pos++;
                  tryParseRequire(RequireType::ExportStar);
                }
              }
            }
            lastTokenPos = pos;
            continue;
        }
      }

      switch (ch) {
        case 'e':
          if (pos + 6 < end && matchesAt(pos + 1, end, "xport") && keywordStart(pos)) {
            if (pos + 7 < end && *(pos + 6) == 's')
              tryParseExportsDotAssign(false);
            else if (openTokenDepth == 0)
              throwIfExportStatement();
          }
          break;
        case 'c':
          if (keywordStart(pos) && matchesAt(pos + 1, end, "lass") && isBrOrWs(*(pos + 5)))
            nextBraceIsClass = true;
          break;
        case 'm':
          if (pos + 6 < end && matchesAt(pos + 1, end, "odule") && keywordStart(pos))
            tryParseModuleExportsDotAssign();
          break;
        case 'O':
          if (pos + 6 < end && matchesAt(pos + 1, end, "bject") && keywordStart(pos))
            tryParseObjectDefineOrKeys(openTokenDepth == 0);
          break;
        case '(':
          openTokenTypeStack_[openTokenDepth] = '(';
          openTokenPosStack_[openTokenDepth++] = lastTokenPos;
          break;
        case ')':
          if (openTokenDepth == 0) {
            syntaxError(lexer_error::UNEXPECTED_PAREN);
            return false;
          }
          openTokenDepth--;
          break;
        case '{':
          openClassPosStack[openTokenDepth] = nextBraceIsClass;
          nextBraceIsClass = false;
          openTokenTypeStack_[openTokenDepth] = '{';
          openTokenPosStack_[openTokenDepth++] = lastTokenPos;
          break;
        case '}':
          if (openTokenDepth == 0) {
            syntaxError(lexer_error::UNEXPECTED_BRACE);
            return false;
          }
          if (openTokenDepth-- == templateDepth) {
            templateDepth = templateStack_[--templateStackDepth];
            templateString();
          } else {
            if (templateDepth != std::numeric_limits<uint16_t>::max() && openTokenDepth < templateDepth) {
              syntaxError(lexer_error::UNTERMINATED_TEMPLATE_STRING);
              return false;
            }
          }
          break;
        case '\'':
        case '"':
          stringLiteral(ch);
          break;
        case '/': {
          char next_ch = pos + 1 < end ? *(pos + 1) : '\0';
          if (next_ch == '/') {
            lineComment();
            continue;
          } else if (next_ch == '*') {
            blockComment();
            continue;
          } else {
            // Check if lastTokenPos is before the source (start of input)
            bool isStartOfInput = lastTokenPos < source;
            char lastToken = isStartOfInput ? '\0' : *lastTokenPos;

            if ((isExpressionPunctuator(lastToken) &&
                 !(lastToken == '.' && lastTokenPos > source && *(lastTokenPos - 1) >= '0' && *(lastTokenPos - 1) <= '9') &&
                 !(lastToken == '+' && lastTokenPos > source && *(lastTokenPos - 1) == '+') &&
                 !(lastToken == '-' && lastTokenPos > source && *(lastTokenPos - 1) == '-')) ||
                (lastToken == ')' && isParenKeyword(openTokenPosStack_[openTokenDepth])) ||
                (lastToken == '}' && (openTokenPosStack_[openTokenDepth] < source || isExpressionTerminator(openTokenPosStack_[openTokenDepth]) || openClassPosStack[openTokenDepth])) ||
                (lastToken == '/' && lastSlashWasDivision) ||
                (!isStartOfInput && isExpressionKeyword(lastTokenPos)) ||
                !lastToken || isStartOfInput) {
              regularExpression();
              lastSlashWasDivision = false;
            } else {
              lastSlashWasDivision = true;
            }
          }
          break;
        }
        case '`':
          if (templateDepth == std::numeric_limits<uint16_t>::max() - 1) {
            syntaxError(lexer_error::TEMPLATE_NEST_OVERFLOW);
            return false;
          }
          templateString();
          break;
      }
      lastTokenPos = pos;
    }

    if (!last_error) {
      if (templateDepth != std::numeric_limits<uint16_t>::max()) {
        syntaxError(lexer_error::UNTERMINATED_TEMPLATE_STRING, end);
      } else if (openTokenDepth != 0) {
        const char open_ch = openTokenTypeStack_[openTokenDepth - 1];
        if (open_ch == '{') {
          syntaxError(lexer_error::UNTERMINATED_BRACE, end);
        } else {
          syntaxError(lexer_error::UNTERMINATED_PAREN, end);
        }
      }
    }

    if (templateDepth != std::numeric_limits<uint16_t>::max() || openTokenDepth || last_error) {
      return false;
    }

    return true;
  }
};

std::optional<lexer_analysis> parse_commonjs(std::string_view file_contents) {
  last_error.reset();
  last_error_location.reset();

  lexer_analysis result;
  CJSLexer lexer(result.exports, result.re_exports);

  if (lexer.parse(file_contents)) {
    return result;  // NRVO or implicit move applies
  }

  return std::nullopt;
}

const std::optional<lexer_error>& get_last_error() {
  return last_error;
}

const std::optional<error_location>& get_last_error_location() {
  return last_error_location;
}

}  // namespace lexer
/* end file src/parser.cpp */
/* begin file src/merve_c.cpp */
/* begin file include/merve_c.h */
/**
 * @file merve_c.h
 * @brief Includes the C definitions for merve. This is a C file, not C++.
 */
#ifndef MERVE_C_H
#define MERVE_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Non-owning string reference.
 *
 * The data pointer is NOT null-terminated. Always use the length field.
 *
 * The data is valid as long as:
 * - The merve_analysis handle that produced it has not been freed.
 * - For string_view-backed exports: the original source buffer is alive.
 */
typedef struct {
  const char* data;
  size_t length;
} merve_string;

/**
 * @brief Opaque handle to a CommonJS parse result.
 *
 * Created by merve_parse_commonjs(). Must be freed with merve_free().
 */
typedef void* merve_analysis;

/**
 * @brief Version number components.
 */
typedef struct {
  int major;
  int minor;
  int revision;
} merve_version_components;

/**
 * @brief Source location for a parse error.
 *
 * - line and column are 1-based.
 * - column is byte-oriented.
 *
 * A zeroed location (`{0, 0}`) means the location is unavailable.
 */
typedef struct {
  uint32_t line;
  uint32_t column;
} merve_error_loc;

/* Error codes corresponding to lexer::lexer_error values. */
#define MERVE_ERROR_TODO 0
#define MERVE_ERROR_UNEXPECTED_PAREN 1
#define MERVE_ERROR_UNEXPECTED_BRACE 2
#define MERVE_ERROR_UNTERMINATED_PAREN 3
#define MERVE_ERROR_UNTERMINATED_BRACE 4
#define MERVE_ERROR_UNTERMINATED_TEMPLATE_STRING 5
#define MERVE_ERROR_UNTERMINATED_STRING_LITERAL 6
#define MERVE_ERROR_UNTERMINATED_REGEX_CHARACTER_CLASS 7
#define MERVE_ERROR_UNTERMINATED_REGEX 8
#define MERVE_ERROR_UNEXPECTED_ESM_IMPORT_META 9
#define MERVE_ERROR_UNEXPECTED_ESM_IMPORT 10
#define MERVE_ERROR_UNEXPECTED_ESM_EXPORT 11
#define MERVE_ERROR_TEMPLATE_NEST_OVERFLOW 12

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse CommonJS source code and optionally return error location.
 *
 * The source buffer must remain valid while accessing string_view-backed
 * export names from the returned handle.
 *
 * If @p out_err is non-NULL, it is always written:
 * - On success: set to {0, 0}.
 * - On parse failure with known location: set to that location.
 * - On parse failure without available location: set to {0, 0}.
 *
 * You must call merve_free() on the returned handle when done.
 *
 * @param input   Pointer to the JavaScript source (need not be
 *                null-terminated). NULL is treated as an empty string.
 * @param length  Length of the input in bytes.
 * @param out_err Optional output pointer for parse error location.
 * @return A handle to the parse result, or NULL on out-of-memory.
 *         Use merve_is_valid() to check if parsing succeeded.
 */
#ifdef __cplusplus
merve_analysis merve_parse_commonjs(const char* input, size_t length,
                                    merve_error_loc* out_err = nullptr);
#else
merve_analysis merve_parse_commonjs(const char* input, size_t length,
                                    merve_error_loc* out_err);
#endif

/**
 * Check whether the parse result is valid (parsing succeeded).
 *
 * @param result Handle returned by merve_parse_commonjs(). NULL returns false.
 * @return true if parsing succeeded, false otherwise.
 */
bool merve_is_valid(merve_analysis result);

/**
 * Free a parse result and all associated memory.
 *
 * @param result Handle returned by merve_parse_commonjs(). NULL is a no-op.
 */
void merve_free(merve_analysis result);

/**
 * Get the number of named exports found.
 *
 * @param result A parse result handle. NULL returns 0.
 * @return Number of exports, or 0 if result is NULL or invalid.
 */
size_t merve_get_exports_count(merve_analysis result);

/**
 * Get the number of re-export module specifiers found.
 *
 * @param result A parse result handle. NULL returns 0.
 * @return Number of re-exports, or 0 if result is NULL or invalid.
 */
size_t merve_get_reexports_count(merve_analysis result);

/**
 * Get the name of an export at the given index.
 *
 * @param result A valid parse result handle.
 * @param index  Zero-based index (must be < merve_get_exports_count()).
 * @return Non-owning string reference. Returns {NULL, 0} on error.
 */
merve_string merve_get_export_name(merve_analysis result, size_t index);

/**
 * Get the 1-based source line number of an export.
 *
 * @param result A valid parse result handle.
 * @param index  Zero-based index (must be < merve_get_exports_count()).
 * @return 1-based line number, or 0 on error.
 */
uint32_t merve_get_export_line(merve_analysis result, size_t index);

/**
 * Get the module specifier of a re-export at the given index.
 *
 * @param result A valid parse result handle.
 * @param index  Zero-based index (must be < merve_get_reexports_count()).
 * @return Non-owning string reference. Returns {NULL, 0} on error.
 */
merve_string merve_get_reexport_name(merve_analysis result, size_t index);

/**
 * Get the 1-based source line number of a re-export.
 *
 * @param result A valid parse result handle.
 * @param index  Zero-based index (must be < merve_get_reexports_count()).
 * @return 1-based line number, or 0 on error.
 */
uint32_t merve_get_reexport_line(merve_analysis result, size_t index);

/**
 * Get the error code from the last merve_parse_commonjs() call.
 *
 * @return One of the MERVE_ERROR_* constants, or -1 if the last parse
 *         succeeded.
 * @note This is global state, overwritten by each merve_parse_commonjs() call.
 */
int merve_get_last_error(void);

/**
 * Get the merve library version string.
 *
 * @return Null-terminated version string (e.g. "1.0.1"). Never NULL.
 */
const char* merve_get_version(void);

/**
 * Get the merve library version as individual components.
 *
 * @return Struct with major, minor, and revision fields.
 */
merve_version_components merve_get_version_components(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MERVE_C_H */
/* end file include/merve_c.h */

#include <new>

struct merve_analysis_impl {
  std::optional<lexer::lexer_analysis> result{};
};

static merve_string merve_string_create(const char* data, size_t length) {
  merve_string out{};
  out.data = data;
  out.length = length;
  return out;
}

static void merve_error_loc_clear(merve_error_loc* out_err) {
  if (!out_err) return;
  out_err->line = 0;
  out_err->column = 0;
}

static void merve_error_loc_set(merve_error_loc* out_err,
                                const lexer::error_location& loc) {
  if (!out_err) return;
  out_err->line = loc.line;
  out_err->column = loc.column;
}

extern "C" {

merve_analysis merve_parse_commonjs(const char* input, size_t length,
                                    merve_error_loc* out_err) {
  merve_error_loc_clear(out_err);

  merve_analysis_impl* impl = new (std::nothrow) merve_analysis_impl();
  if (!impl) return nullptr;
  if (input != nullptr) {
    impl->result = lexer::parse_commonjs(std::string_view(input, length));
  } else {
    impl->result = lexer::parse_commonjs(std::string_view("", 0));
  }

  if (!impl->result.has_value() && out_err) {
    const std::optional<lexer::error_location>& err_loc =
        lexer::get_last_error_location();
    if (err_loc.has_value()) {
      merve_error_loc_set(out_err, err_loc.value());
    }
  }

  return static_cast<merve_analysis>(impl);
}

bool merve_is_valid(merve_analysis result) {
  if (!result) return false;
  return static_cast<merve_analysis_impl*>(result)->result.has_value();
}

void merve_free(merve_analysis result) {
  if (!result) return;
  delete static_cast<merve_analysis_impl*>(result);
}

size_t merve_get_exports_count(merve_analysis result) {
  if (!result) return 0;
  merve_analysis_impl* impl = static_cast<merve_analysis_impl*>(result);
  if (!impl->result.has_value()) return 0;
  return impl->result->exports.size();
}

size_t merve_get_reexports_count(merve_analysis result) {
  if (!result) return 0;
  merve_analysis_impl* impl = static_cast<merve_analysis_impl*>(result);
  if (!impl->result.has_value()) return 0;
  return impl->result->re_exports.size();
}

merve_string merve_get_export_name(merve_analysis result, size_t index) {
  if (!result) return merve_string_create(nullptr, 0);
  merve_analysis_impl* impl = static_cast<merve_analysis_impl*>(result);
  if (!impl->result.has_value()) return merve_string_create(nullptr, 0);
  if (index >= impl->result->exports.size())
    return merve_string_create(nullptr, 0);
  std::string_view sv =
      lexer::get_string_view(impl->result->exports[index]);
  return merve_string_create(sv.data(), sv.size());
}

uint32_t merve_get_export_line(merve_analysis result, size_t index) {
  if (!result) return 0;
  merve_analysis_impl* impl = static_cast<merve_analysis_impl*>(result);
  if (!impl->result.has_value()) return 0;
  if (index >= impl->result->exports.size()) return 0;
  return impl->result->exports[index].line;
}

merve_string merve_get_reexport_name(merve_analysis result, size_t index) {
  if (!result) return merve_string_create(nullptr, 0);
  merve_analysis_impl* impl = static_cast<merve_analysis_impl*>(result);
  if (!impl->result.has_value()) return merve_string_create(nullptr, 0);
  if (index >= impl->result->re_exports.size())
    return merve_string_create(nullptr, 0);
  std::string_view sv =
      lexer::get_string_view(impl->result->re_exports[index]);
  return merve_string_create(sv.data(), sv.size());
}

uint32_t merve_get_reexport_line(merve_analysis result, size_t index) {
  if (!result) return 0;
  merve_analysis_impl* impl = static_cast<merve_analysis_impl*>(result);
  if (!impl->result.has_value()) return 0;
  if (index >= impl->result->re_exports.size()) return 0;
  return impl->result->re_exports[index].line;
}

int merve_get_last_error(void) {
  const std::optional<lexer::lexer_error>& err = lexer::get_last_error();
  if (!err.has_value()) return -1;
  return static_cast<int>(err.value());
}

const char* merve_get_version(void) { return MERVE_VERSION; }

merve_version_components merve_get_version_components(void) {
  merve_version_components vc{};
  vc.major = lexer::MERVE_VERSION_MAJOR;
  vc.minor = lexer::MERVE_VERSION_MINOR;
  vc.revision = lexer::MERVE_VERSION_REVISION;
  return vc;
}

}  /* extern "C" */
/* end file src/merve_c.cpp */
